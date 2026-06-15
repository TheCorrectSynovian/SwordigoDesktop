# Swordigo Native Symbol Map

## 1. Core Engine (Caver Namespace)
The engine is built around the `Caver` namespace, following a "Shell" and "Context" architecture.

### 1.1 Application Management
- `Caver::CaverShell`: Main engine controller.
  - `InitApplication()`
  - `Update(float)`
  - `Render(Caver::RenderingContext*)`
  - `QuitApplication()`
- `Caver::FWShell`: Framework abstraction.
  - `ApplicationShouldQuit()`
  - `SetResourcesPath(Caver::String const&)`

### 1.2 Rendering & Camera
- `Caver::RenderingContext`: Manages GLES state.
  - `Clear(bool, bool, bool)`
  - `SetClearColor(Caver::Color const&)`
- `Caver::Camera`: 
  - `SetPosition(Caver::Vector3 const&)`
  - `SetFocus(Caver::Entity*)`

### 1.3 Entity Component System (ECS)
The engine uses a component-based model.
- `Caver::Entity`: Base object class.
- `Caver::Component`: Base component class.
- **Notable Components**:
  - `Caver::MonsterEntityComponent`: Logic for enemies.
  - `Caver::ProgramComponent`: Scripting/Program execution.
  - `Caver::PhysicsComponent`: Collision and movement.
  - `Caver::EntityControllerComponent`: Interface for movement/actions.

## 2. Scripting & Logic
- `Caver::Program`: Manages high-level game logic/scripts (Lua-backed).
- `Caver::ProgramState`: Holds execution context.

## 3. Managers & Systems
- `Caver::Scene`: Manages the current level and its entities.
- `Caver::MusicPlayer`: Native bridge for audio.
- `Caver::PlayerProfile`: Manages save data (`.gstate`).

## 4. Utilities
- `Caver::Matrix4`: 4x4 Math matrix.
- `Caver::Vector3`: 3D Vector.
- `Caver::Color`: RGBA Color.

## 5. Observed JNI Exports
- `Java_com_touchfoo_swordigo_Native_setupApplication`
- `Java_com_touchfoo_swordigo_Native_setupNativeInterface`
- `Java_com_touchfoo_swordigo_Native_updateApplication`
- `Java_com_touchfoo_swordigo_Native_drawApplication`
- `Java_com_touchfoo_swordigo_Native_setAssetManager`
- `Java_com_touchfoo_swordigo_Native_setFilesDir`
- `Java_com_touchfoo_swordigo_Native_setCacheDir`
