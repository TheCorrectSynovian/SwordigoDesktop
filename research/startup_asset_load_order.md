# Startup Asset Load Order

This document maps the exact asset loading chain from `setupApplication()` to the first rendered frame in *Swordigo* (version 1.4.6), identifying the critical asset request that proves the engine has successfully transitioned to the rendering phase.

---

## 1. The Startup Asset Chain

When `setupApplication()` is called, the C++ engine (`CaverShell`) initializes resources and libraries in the following sequential order:

```
[setupApplication JNI Call]
         │
         ▼
[1. sounds.sounds]                 <─── Configures the SoundLibrary (First file read)
         │
         ▼
[2. gamedata.gdata]                <─── Parses global Quests, Items, and Triggers database
         │
         ▼
[3. game_common.scl]               <─── Loads base entity blueprint templates
         │
         ▼
[4. game_common_atlas_2x.pvr]      <─── FIRST TEXTURE LOADED (GUI sprite sheet)
         │
         ▼
[5. font_megalopolis_12.fnt]       <─── FIRST FONT LOADED (Default UI font)
         │
         ▼
[6. menu.scene]                    <─── FIRST SCENE LOADED (Main Menu scene definition)
         │
         ▼
[7. menu_back.POD]                 <─── FIRST MODEL LOADED (Background mountains/sky geometry)
         │
         ▼
[8. char_evil.POD]                 <─── Character model (darkhero) loaded
         │
         ▼
[9. hiro_stand.POD]                <─── Stance animation for darkhero loaded
         │
         ▼
[10. swordigo_title_2x.pvr]        <─── Title Logo Texture (Confirms rendering phase)
```

---

## 2. Detailed Asset Chain Analysis

### Step 1: Base Configurations & Templates
*   **`sounds.sounds`**: The engine opens this text configuration first to map string sound effect keys (e.g. `coin_get`) to raw `.wav` filenames.
*   **`gamedata.gdata`**: Parsed via `Caver::LoadProtobufMessageFromFile` to define the global database of items, quests, triggers, and maps.
*   **`game_common.scl`**: Loaded next to register entity blueprints (health parameters, speed settings, components). *Note: The `.POD` model files referenced in the `.scl` templates are NOT loaded yet; they are only cached as paths.*

### Step 2: Texture Atlas & Fonts
*   **`game_common_atlas_2x.pvr` (First Texture Loaded)**: Before any scene loads, the engine preloads `game_common_atlas_2x.atlas` to define the screen boundaries of general UI buttons, health hearts, and borders. This triggers the load of `game_common_atlas_2x.pvr`.
*   **`font_megalopolis_12.fnt` (First Font Loaded)**: Parsed next to register the character glyph layout for rendering menu text, alongside its backing texture `font_megalopolis_12_2x.pvr`.

### Step 3: Scene Setup & Models
*   **`menu.scene` (First Scene Loaded)**: The first scene loaded to present the main menu viewport.
*   **`menu_back.POD` (First Model Loaded)**: The very first object in `menu.scene` is the `Background` entity. Building this entity triggers the engine to load the mountainous backdrop model `menu_back.POD` and its texture `menu_back.pvr`.
*   **Character Models**: The engine subsequently constructs `darkhero` (loading `char_evil.POD` and `hiro_stand.POD`) and `sword` (loading `weapon_glow.POD`).

---

## 3. The Rendering Verification Asset

> [!IMPORTANT]
> **Which asset request proves the engine has successfully reached the rendering phase?**
>
> **`swordigo_title_2x.pvr`**
>
> *   *Why*: The main logo texture (`swordigo_title_2x.pvr`) is **not** part of the scene graph defined in `menu.scene` (it is not a 3D model placed in the world). Instead, it is loaded dynamically by the UI overlay class (`Caver::MainMenuView`) when it initializes its drawing assets to render the actual title overlay.
> *   *Significance*: An asset request for `swordigo_title_2x.pvr` confirms that:
>     1.  `setupApplication()` has successfully parsed the configuration and level scenes.
>     2.  The engine has initialized the UI layout views and is preparing to output pixels to the screen.
