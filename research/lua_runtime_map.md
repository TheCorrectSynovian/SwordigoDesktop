# Lua Gameplay Runtime Map

This document outlines the architecture, lifecycles, execution models, and component relationships of the Lua gameplay script engine in *Swordigo* (version 1.4.6).

---

## 1. Lua State Lifecycle & Script Execution

The Lua VM is managed natively by the C++ class `Caver::ProgramState` which wraps the standard `lua_State*` interpreter state.

### VM Lifecycle Managers
*   **State Creation**: `00337235` (`_Z13luaL_newstatev`)
    *   Called during the construction of the root `Caver::ProgramState` object (`00319531`). This initializes a new `lua_State` pointer.
*   **State Destruction**: `00333ac1` (`_Z9lua_closeP9lua_State`)
    *   Called during the destruction of the `Caver::ProgramState` object (`0031969d`). This frees the Lua interpreter state and cleans up VM memory.
*   **API Registration**: `00319b93` (`_ZN5Caver12ProgramState15RegisterLibraryERKSsPKNS_11LibFunctionE`)
    *   Called during initialization to bind host C++ functions (using `lua_pushcclosure` at `0032e1e5` and `lua_setfield`) to expose libraries (e.g. `MusicPlayer`, `Scene`, `Camera`) to Lua.

### Script Loading & Execution
*   **Loading Scripts**: `003199ab` (`_ZN5Caver12ProgramState11LoadProgramERKNS_7ProgramE`)
    *   Translates a `Caver::Program` resource (loaded from protobuf files) and compiles the Lua source code string or binary bytecode (`LuaQ`) into the VM using `luaL_loadbuffer` (`00337185`) or `lua_load` (`0032e7d9`).
*   **Running Scripts**: `003199ed` (`_ZN5Caver12ProgramState14ExecuteProgramERKNS_7ProgramE`)
    *   Executes the compiled program block inside the VM using the protected call function `lua_pcall` (`0032e69d`).
*   **Executing Direct Strings**: `00319991` (`_ZN5Caver12ProgramState13ExecuteStringERKSs`)
    *   Compiles and runs a raw C++ string of Lua code. This function is vital for creating a debugging harness.

---

## 2. Scripting Component Architecture

The scripting engine follows a strict Entity Component System (ECS) model:

```
[SceneObject (Entity)]
       │
       ▼ (contains)
[ProgramComponent (001de309)] ───► holds ───► [Program (00319205)] ───► runs ──► [Lua Code]
       │
       ▼ (invokes)
[ProgramState (00319531)] ───────► wraps ───► [lua_State]
```

1.  **`Caver::Program`**: Holds the binary compiled bytecode or plaintext source code string parsed from scene assets.
2.  **`Caver::ProgramState`**: Exposes the execution context thread (`lua_State`). It runs child threads (coroutines) for individual entity actions, using `ProgramState::Wait(float)` to yield script execution.
3.  **`Caver::ProgramComponent`**: A C++ component that wraps the script execution context. It registers event triggers (such as region entries, entity deaths, or timer expirations) and invokes `ProgramComponent::Execute()` to run the Lua script when triggered.

---

## 3. Engine Test Harness (Custom Script Execution)

The existence of `Caver::ProgramState::ExecuteString` provides a straightforward path to execute custom Lua scripts on the emulated engine *without* loading the full game assets.

### Execution Plan
To test shims (e.g., verifying the OpenAL audio player), the host loader can inject and call a custom Lua script:

1.  **Setup VM**: Construct a root `Caver::ProgramState` and register the engine libraries (`ProgramState::RegisterProgramLibrary()`).
2.  **Allocate Script String**: Write a custom script into guest RAM (e.g., a `std::string` wrapper containing `MusicPlayer.PlayMusic("menu")`).
3.  **Trigger Execution**:
    *   Set register `R0` to the guest address of the `ProgramState` object.
    *   Set register `R1` to the guest address of the `std::string` buffer.
    *   Call `Caver::ProgramState::ExecuteString(std::string const&)` (`00319991`).
4.  **Result**: The guest Lua VM executes the script, calling the C++ music player shims directly.
