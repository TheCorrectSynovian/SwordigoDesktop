# Swordigo Data Dependency Map

This document maps the relationships between various game resources and systems.

## Resource Hierarchy

### 1. Raw Assets (The Leaf Nodes)
- **Visuals**: `.pvr` (Textures), `.POD` (PowerVR Object Data / 3D Models).
- **Audio**: `.wav` (Sound Effects).
- **Configuration**: `.sounds`, `.atlas`, `.fnt`.

### 2. Entity Templates (`.scl`)
- **Blueprints**: Define shared properties for entities (monsters, props, platforms).
- **Dependencies**: Reference **Raw Assets** (models, animations).
- **Usage**: Used by `.scene` files to instantiate objects.
- **Example**: `monsters.scl` defines "Cave Bat", which references `bat_red.pvr` and `bat_fly.POD`.

### 3. World Definition (`.scene`)
- **Instance Data**: Specific placement (x, y, rotation) of entities in a level.
- **Logic**: Contains embedded **Lua Scripts** for instance-specific behavior and triggers.
- **Dependencies**: References **Entity Templates** (`.scl`) for base definitions.
- **Example**: `forest_part1.scene` places multiple "Cave Bat" instances.

### 4. Global Game Database (`gamedata.gdata`)
- **Registries**: Central list of all Items, Spells, Quests, and World Triggers.
- **Logic Mapping**: Links specific Quests to target Scenes and Trigger IDs within those scenes.
- **Example**: Defines `quest03_woodkeep` and points it to `plains_woodkeep3.scene`.

### 5. Player Progress (`player.gstate`)
- **Dynamic State**: Tracks current inventory, unlocked spells, and equipped gear.
- **Location**: Stores the current Scene and Spawn Point.
- **Dependencies**: References **Game Data** (item/quest IDs) and **Scenes**.

## System Flow

Assets (.pvr, .POD, .wav)
      |
      v
Entity Templates (.scl)  <-- [Blueprints]
      |
      v
   Scenes (.scene)       <-- [Instances + Lua Logic]
      ^
      |
Game Database (.gdata)   <-- [Quests, Items, Triggers]
      ^
      |
 Player State (.gstate)  <-- [Inventory, Progress, Location]

## Runtime Architecture

1. **Android/Host Layer**: 
   - Provides `AssetManager` for file access.
   - Forwards input to `Native.handleTouchEvent`.
   - Manages audio lifecycle via `MusicPlayer.java`.
2. **Native Engine (`libswordigo.so`)**:
   - Parses Protobuf assets using an internal library (`Caver::LoadProtobufMessageFromFile`).
   - Manages the Scene Graph and Entity Component System (`Caver::CaverShell`).
   - Executes the **Lua VM** for gameplay logic found in `.scene` scripts.
   - Renders using **OpenGL ES 1.1**.
   - Handles physics and collisions via native components.
