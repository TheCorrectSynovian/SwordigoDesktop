# Native Engine Map (libswordigo.so 1.4.6)

## Major Subsystems (Namespace `Caver`)

### 1. Engine Core
- `Caver::CaverShell`: Orchestrates the main loop (`Update`, `Render`).
- `Caver::FWShell`: Framework-level abstractions (Paths, Quit).

### 2. Scene Graph
- `Caver::Scene`: Level management.
- `Caver::SceneObject`: Entity instances.
- `Caver::Entity`: Base gameplay object.
- `Caver::Component`: Base data component.

### 3. Scripting (Lua)
- `Caver::Program`: Handles high-level scripts.
- `Caver::ProgramState`: Lua integration.
- Symbols like `Caver::ProgramState::FromLuaState` and `luaL_newstate` confirm a standard Lua 5.1/LuaJIT core.

### 4. Data & Resources
- `Caver::GameData`: Global database (`.gdata`).
- `Caver::GameState`: Current session data (`.gstate`).
- `Caver::ModelLibrary`: Manages 3D models (`.POD`).
- `Caver::PODLoader`: Specifically handles PowerVR Object Data.

### 5. Utilities
- `Caver::Matrix4`, `Caver::Vector3`, `Caver::Color`.

## Lua Registration
The following native functions are likely registered to Lua:
- `EntityController::*` (Movement, Actions)
- `Scene::*` (Find, CreateObject)
- `Game::*` (CinematicMode)
- `Camera::*` (FocusAtPoint)
