# Startup Scene Dependency Map

This document details the startup scene, camera targets, UI overlays, resource dependencies, and Lua scripting for the main menu of *Swordigo* (version 1.4.6).

---

## 1. Startup & Menu Scene Definitions

*   **Startup Scene File**: `assets/resources/menu.scene`
*   **First Menu Scene**: `assets/resources/menu.scene`
    *   This protobuf wire-format file defines the main menu environment, lighting, mesh geometry, and background entities.
*   **First Camera Entity**: `obj3`
    *   The engine's active camera (`Caver::Camera`) does not exist as an entity within `menu.scene`. Instead, `obj3` is an invisible `UtilityShape` that acts as the camera's focus and follow target.
*   **First UI Entity**: `Caver::MainMenuView`
    *   The main menu overlay is managed in C++ by the view class `Caver::MainMenuView`, which renders the interactive UI buttons (Play, Options, Music toggle) and overlays.
    *   If the user clicks "Play", the engine instantiates `Caver::ProfilePanelView` to display the three local save slot selections.

---

## 2. Startup Scene Entity Hierarchy (`menu.scene`)

The scene graph inside `menu.scene` contains the following entities:

1.  **`Background`**: The sky backdrop entity.
    *   *Components*: `Background` component mapping to model `"menu_back"`.
2.  **`DirectionalLight`**: Provides scene illumination.
    *   *Components*: Three distinct `Light` components (ambient, diffuse, directional).
3.  **`darkhero`**: The shadow warrior character displayed on the menu.
    *   *Components*:
        *   Model: `"char_evil"`
        *   KeyframeAnimation: `"hiro_stand"`
        *   MonsterEntity
        *   EntityController
        *   AnimationController
4.  **`sword`**: The weapon held by `darkhero`.
    *   *Components*: Model: `"weapon_glow"` (broadsword mesh).
5.  **`obj1`, `obj1#2`, `obj1#3`**: Background environment trees.
    *   *Components*: Models: `"treek"`, `"treek"`, `"tree1"`.
6.  **`obj2`**: Foreground mountains and rocky cliffs.
    *   *Components*: `GroundPolygon` and `GroundMesh` rendering terrain using `"menu_back"` texture indices.
7.  **`obj3`**: Invisible camera controller.
    *   *Components*:
        *   `UtilityShape`
        *   `PhysicsObject`
        *   `Program` (executes the camera-panning Lua script).
8.  **`spawn_default`**: Default level spawn coordinates.
    *   *Components*: Position coordinate helper.

---

## 3. Resource & Asset Dependencies

For `menu.scene` to render and function, the following raw asset files are loaded first:

### 3D Models & Animations (`.POD`)
*   `assets/resources/menu_back.POD`: Background geometry.
*   `assets/resources/char_evil.POD`: The mesh definition for the dark hero character.
*   `assets/resources/hiro_stand.POD`: Idle stance animation for the character.
*   `assets/resources/treek.POD` & `tree1.POD`: Tree mesh definitions.
*   `assets/resources/weapon_glow.POD`: Mesh for the sword.

### Textures (`.pvr`)
*   `assets/resources/menu_back.pvr`: Sky and background textures.
*   `assets/resources/game_common_atlas_2x.pvr`: UI sprites, buttons, borders, and icon textures.
*   `assets/resources/swordigo_title_2x.pvr`: Main logo graphic.

### Fonts (`.fnt`)
*   `assets/resources/font_megalopolis_10.fnt` & `font_megalopolis_10_2x.pvr`
*   `assets/resources/font_megalopolis_12.fnt` & `font_megalopolis_12_2x.pvr`
*   `assets/resources/font_megalopolis_14.fnt` & `font_megalopolis_14_2x.pvr`
*   `assets/resources/font_megalopolis_18.fnt` & `font_megalopolis_18_2x.tex.png`

---

## 4. Embedded Main Menu Lua Script

The camera movement, bone attachment, and menu theme playback are controlled by a Lua script embedded directly inside the `Program` component of `obj3`.

### Lua Source Code
```lua
local self = ...;

-- Attach the broadsword model to the right weapon bone of the dark hero
ObjectLinkController.LinkToBone(Scene.Find("sword"), Scene.Find("darkhero"), "BoneRightWeapon");

-- Start background music theme
MusicPlayer.PlayMusic("menu");

local time = 0;
local center = self:position();

-- Calculate an elliptical path for the camera focal target
function PositionAtTime(time)
	local direction = Vector3.FromAngle(time * 20);
	local position = center + Vector3.New(direction:x() * 20, direction:y() * 10, 0);
	return position;
end

-- Initialize camera focus parameters
self:setPosition(PositionAtTime(time));
Camera.FocusAtShape(self);
Camera.JumpToFocus();
Camera.FollowShape(self);

-- Continuous camera panning orbit
while true do 
	time = time + 0.01;
	Program.Wait(0.01);
	self:setPosition(PositionAtTime(time));
end
```

### Script Execution Mechanics
*   **Target Linking**: Links the `sword` entity to the `darkhero` skeleton model bone `"BoneRightWeapon"` via `ObjectLinkController`.
*   **Theme Playback**: Invokes `MusicPlayer.PlayMusic("menu")`.
*   **Focal Target Orbit**: The script updates `obj3`'s position in a continuous elliptical path:
    $$x = center.x + 20 \cdot \cos(time \cdot 20)$$
    $$y = center.y + 10 \cdot \sin(time \cdot 20)$$
*   **Camera Tracking**: Since the camera focuses on and follows `self` (`obj3`), this circular panning gives the main menu its dynamic animated background.
