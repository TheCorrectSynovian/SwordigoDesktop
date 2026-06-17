# Lua Scripting & Protobuf Schema Reconnaissance

This report documents the embedded scripts in *Swordigo* (version 1.4.6), outlines the test harness to run custom Lua code, and reconstructs the protobuf schemas based on engine symbols.

---

## 1. Gameplay Script Reconnaissance & First Menu Script

A search of the assets folder confirms that Lua scripts are **never** stored as standalone `.lua` files on disk. Instead, they are embedded directly inside the `.scene` database files in both raw text and precompiled bytecode (`\033LuaQ`).

### Location of the First Menu Script
*   **Asset File**: `assets/resources/menu.scene`
*   **Hosting Entity**: `obj3` (an invisible `UtilityShape` serving as the camera focal target).
*   **Component**: `ProgramComponent` (maps to extension field `3.157`).
*   **Provisional Fields**:
    *   `157.2.1` / `121.9.1`: Plaintext Lua source.
    *   `157.2.2` / `121.9.2`: Precompiled Lua 5.1 bytecode (`LuaQ`).

### First Menu Script Walkthrough
The script running at boot in `menu.scene` performs three critical operations:
1.  **Weapon Attachment**:
    ```lua
    ObjectLinkController.LinkToBone(Scene.Find("sword"), Scene.Find("darkhero"), "BoneRightWeapon");
    ```
    Links the `sword` model model to the right hand weapon bone of the `darkhero` character skeleton.
2.  **Audio Theme Playback**:
    ```lua
    MusicPlayer.PlayMusic("menu");
    ```
    Requests the JNI audio player to start playing the menu theme music.
3.  **Camera orbit / pan**:
    ```lua
    self:setPosition(PositionAtTime(time));
    Camera.FocusAtShape(self);
    Camera.JumpToFocus();
    Camera.FollowShape(self);
    ```
    Points the active camera to focus on and follow `self` (`obj3`), then enters a loop shifting `self`'s coordinates in an ellipse around the screen to create the animated background panning effect.

---

## 2. Engine Test Harness (Stretch Goal)

We can force the engine to run custom scripts without loading a menu scene. This is a valuable tool for testing shims.

### Execution Blueprint
By utilizing the engine's internal `Caver::ProgramState::ExecuteString(std::string const&)` function (`00319991`), the guest runner can execute arbitrary strings of Lua code:

1.  **Initialize VM**: Allocate a root `ProgramState` and call `RegisterProgramLibrary()` to load default namespaces.
2.  **Allocate Guest String**: Create a `std::string` object in guest memory wrapping the target script:
    ```lua
    -- Test play command directly
    MusicPlayer.PlayMusic("menu");
    ```
3.  **Branch & Link**:
    *   Set register `R0` to the address of the `ProgramState` object.
    *   Set register `R1` to the address of the allocated `std::string`.
    *   Set the PC to `0x00319991` and branch.
4.  **Verification**: The guest interpreter compiles and runs the string, immediately calling the host's `MusicPlayerJNI::Play()` shim.

---

## 3. Reconstructed Protobuf Schema (Mission 15)

By correlating `protoc --decode_raw` outputs with `Caver::Proto::*` symbols, the following protobuf schemas are reconstructed for the game assets:

```protobuf
syntax = "proto2";
package Caver.Proto;

// --- 1. Scene Database (.scene) ---
message Scene {
  repeated SceneObject objects = 1;
}

message SceneObject {
  required string name = 2;
  repeated Component components = 3;
  optional Vector3 position = 4;
  optional Vector3 scale = 5;
  optional Vector3 rotation = 6;
  optional Vector3 velocity = 7;
  optional Rectangle bounds = 8;
}

message Component {
  required string type = 1;
  required int32 id = 2;
  
  // Extension fields mapped to component configurations
  optional ModelComponent model = 101;
  optional KeyframeAnimationComponent animation = 102;
  optional ProgramComponent program = 157;
  optional LightComponent light = 130;
}

message Vector3 {
  required float x = 1;
  required float y = 2;
  required float z = 3;
}

message Rectangle {
  required float x = 1;
  required float y = 2;
  required float w = 3;
  required float h = 4;
}

message ProgramComponent {
  required int32 id = 1;
  optional string source_code = 2;
  optional bytes bytecode = 3;
  optional int32 trigger_type = 4;
}

// --- 2. Player Progress & Save State (.gstate) ---
message SaveFile {
  required PlayerProfile profile = 1;
  required string current_scene = 3;
  required string spawn_point = 4;
  optional string map_state = 9;
}

message PlayerProfile {
  required int32 level = 5;
  repeated string inventory = 11;     // e.g. "legendsword", "platearmor"
  repeated string abilities = 15;     // e.g. "bolt", "bomb", "hookshot"
  optional string active_ability = 16;
  optional string active_trinket = 17;
}

// --- 3. Global Game Catalog (gamedata.gdata) ---
message GameCatalog {
  repeated CatalogEntry entries = 1;
}

message CatalogEntry {
  required int32 category = 1;        // 2: Items, 3: Quests, 5: Quest Triggers
  required string identifier = 2;     // e.g. "brasssword", "quest03"
  optional string display_name = 3;   // e.g. "Brass Sword"
  optional string stats = 4;
  optional string description = 5;
  optional string target_scene = 6;
}
```
With these schemas, we can read, write, and manipulate game scenes, player save files, and game catalogs using the standard `protoc` compiler.
