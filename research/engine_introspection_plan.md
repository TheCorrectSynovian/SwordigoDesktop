# Engine Introspection Plan

This plan establishes the design, hook specifications, and script APIs to implement a live **Engine Introspection Framework** in the *Swordigo* emulator. This framework enables developers to query Lua globals, active scene entities, and C++ component layouts dynamically.

---

## 1. Hooking & Output Integration Specification

To interact with the engine at runtime, we hook the execution entry points and override standard Lua reporting features.

```mermaid
graph TD
    HostShell[Host Debugger Shell / REPL] -->|1. Inject String| HookExecute[Hook: ProgramState::ExecuteString]
    HookExecute -->|2. Run Lua VM| LuaVM[Internal Lua 5.1 VM]
    LuaVM -->|3. Query State| CppEngine[C++ Game Engine Subsystems]
    LuaVM -->|4. Redirect print()| HookPrint[Hook: Program.Print]
    HookPrint -->|5. Output Results| HostShell
```

### Hook: `ProgramState::ExecuteString`
*   **Address**: `00319991` (1.4.6)
*   **Action**: Hook the entry of this function. On trigger:
    *   Read `R1` (which points to a 12-byte `std::string` containing the script).
    *   Log script contents to host console for execution tracing.

### Output Redirection
To allow standard Lua `print(...)` statements to output directly to the host terminal:
1.  **Lua Override**: Override `_G.print` during initialization:
    ```lua
    _G.print = function(...)
        local args = {...}
        local output = {}
        for i, v in ipairs(args) do
            output[i] = tostring(v)
        end
        Program.Print(table.concat(output, "\t"))
    end
    ```
2.  **C++ Hook**: Intercept the native implementation of `Program.Print` (which is registered in Lua during `ProgramState::RegisterProgramLibrary` at address `003195f5`). The native print function writes to standard output or Android `__android_log_print`, which the emulator captures and displays in the terminal.

---

## 2. Metatable Stringification (`__tostring` Overrides)

By default, printing userdata in Lua returns generic hex pointers (e.g. `userdata: 0x304abf`). We override the `__tostring` metamethod on default class metatables to return detailed status strings.

### SceneObject stringification
```lua
-- Hook metatable for SceneObject references
local obj_meta = debug.getmetatable(Scene.Find("hero")) or {}
obj_meta.__tostring = function(self)
    local pos = self:position()
    local vel = self:velocity()
    return string.format(
        "SceneObject[\"%s\"]:\n  Position: (%.2f, %.2f, %.2f)\n  Velocity: (%.2f, %.2f, %.2f)\n  Alive: %s",
        self:identifier(), pos:x(), pos:y(), pos:z(), vel:x(), vel:y(), vel:z(), tostring(self:isAlive())
    )
end
```

### Vector3 & Rectangle stringification
```lua
local vec_meta = debug.getmetatable(Vector3.New(0,0,0)) or {}
vec_meta.__tostring = function(self)
    return string.format("Vector3(%.2f, %.2f, %.2f)", self:x(), self:y(), self:z())
end
```

---

## 3. Enumerating Global States & Systems

We query the Lua VM state natively to trace all tables, modules, and functions.

### Lua Globals (`dump_globals`)
Dumps all active global variables, excluding standard tables and functions:
```lua
function dump_globals()
    local excluded = { _G = true, _VERSION = true, string = true, table = true, 
                       math = true, debug = true, coroutine = true, os = true }
    print("================ LUA GLOBALS ================")
    for k, v in pairs(_G) do
        if not excluded[k] then
            print(string.format("%-25s : %s", k, type(v)))
        end
    end
end
```

### Registered Libraries (`dump_libraries`)
Iterates over all tables registered in the global environment, listing their functions:
```lua
function dump_libraries()
    print("================ ENGINE LIBRARIES ================")
    for lib_name, lib_table in pairs(_G) do
        if type(lib_table) == "table" and lib_name ~= "_G" and lib_name ~= "package" then
            print("Library: " .. lib_name)
            for func_name, func_val in pairs(lib_table) do
                if type(func_val) == "function" then
                    print("  - " .. func_name)
                end
            end
        end
    end
end
```

---

## 4. Enumerating C++ Engine Objects & Components

To inspect active C++ game entities, we use two methods depending on the debugger context:

### Method A: Hooking Native C++ Functions
We hook `Caver::Scene::GetAllObjects` to enumerate all objects in the level scene:
*   **Function Signature**: `Caver::Scene::GetAllObjects(std::vector<boost::intrusive_ptr<Caver::SceneObject>>*)` (address `002ec315`)
*   **Execution Pattern**:
    1.  Capture the current active `Scene*` pointer (by hooking `Scene::Update` at address `002ea411` and saving `R0`).
    2.  Allocate a C++ vector descriptor in guest memory.
    3.  Call `002ec315` passing `R0 = scene_ptr`, `R1 = vector_addr`.
    4.  Traverse the returned vector to fetch the `this` pointer for every `SceneObject`.

### Method B: Direct Memory Traversal (No-Call Execution)
Instead of invoking C++ functions (which can trigger crashes if parameters are unstable), the emulator can directly parse memory structures:
*   **Scene Object Vector**: The `Scene` object contains a `std::vector` of loaded objects. In 32-bit ARM, a vector consists of 12 bytes (`begin_ptr`, `end_ptr`, `storage_end_ptr`).
*   **Traversal Logic**:
    ```python
    # Locate std::vector<intrusive_ptr<SceneObject>> inside Caver::Scene (Offset Y)
    begin_ptr = emulator.read_ptr(scene_addr + Y)
    end_ptr = emulator.read_ptr(scene_addr + Y + 4)
    
    for ptr in range(begin_ptr, end_ptr, 4):
        scene_object_addr = emulator.read_ptr(ptr)
        identifier_str = emulator.read_cpp_string(scene_object_addr + IDENTIFIER_OFFSET)
        print(f"Object: {identifier_str} @ {hex(scene_object_addr)}")
    ```

---

## 5. Introspection Console Helper Suite

We inject these debug functions into the global `_G` namespace during console startup:

```lua
-- Helper shell suite for developer console
_G.help = function()
    print("================ SWORDIGO DEBUG CONSOLE ================")
    print("help()                - Show this menu")
    print("dump_scene()          - List all objects in the active scene")
    print("dump_globals()        - Print all active Lua global variables")
    print("dump_libraries()      - List all registered libraries and API methods")
    print("inspect(name)         - Inspect details of a specific SceneObject")
end

_G.dump_scene = function()
    print("================ ACTIVE SCENE OBJECTS ================")
    -- If Scene.GetAllObjects was bound to Lua via C++ bridge:
    local objects = Scene.GetAllObjects()
    for _, obj in ipairs(objects) do
        print(string.format("%-20s : Pos(%d,%d,%d)", obj:identifier(), obj:position():x(), obj:position():y(), obj:position():z()))
    end
end

_G.inspect = function(object_name)
    local obj = Scene.Find(object_name)
    if not obj then
        print("Error: Object \"" .. object_name .. "\" not found.")
        return
    end
    print(tostring(obj))
    -- Query basic properties
    print("Properties:")
    print("  Active: " .. tostring(obj:isAlwaysActive()))
    print("  Hidden: " .. tostring(obj:setHidden())) -- queries flag if no arg
end
```
