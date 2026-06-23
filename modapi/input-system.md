# Input System

> **Header**: [`src/platform/input_config.h`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/input_config.h)
> **Source**: [`src/platform/input_config.cpp`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/input_config.cpp)
> **Config file**: `~/.local/share/swordigo-desktop/save/controls_arm64.ini`
> **Toggle editor**: **F2**

The input system translates keyboard, gamepad, and TV remote input into the touch events that the ARM game engine expects. Every physical input is mapped to a **virtual touch button** — a circle in the game's 960×544 coordinate space that simulates a finger tap when its bound key is pressed.

---

## Architecture Overview

```
Keyboard/Gamepad event
    ↓
InputConfig::find_by_scancode() / find_by_gamepad_button()
    ↓
TouchButton (game_x, game_y, touch_id)
    ↓
JNI bridge → swordigo_nativeTouchEvent(action, id, time, x, y, ...)
    ↓
Game engine processes touch as if it came from a real touchscreen
```

All positions use the **960×544 game coordinate space** internally. Window-space conversion happens only at render time.

---

## TouchButton Struct

Each virtual button is represented by a `TouchButton`. These are fully serializable to/from `controls.ini`.

### Identity Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `name` | `std::string` | `""` | Internal ID: `"left"`, `"right"`, `"jump"`, `"magic_bolt"`, `"custom_1"` |
| `display_name` | `std::string` | `""` | User-facing label: `"Move Left"`, `"Magic Bolt"`, `"My Custom Btn"`. If empty, a hardcoded default is used |
| `is_custom` | `bool` | `false` | `true` for user-created buttons; only custom buttons can be deleted |

### Position & Size (960×544 game space)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `game_x` | `float` | `0` | X position in game coordinates (0 = left, 960 = right) |
| `game_y` | `float` | `0` | Y position in game coordinates (0 = top, 544 = bottom) |
| `radius` | `float` | `40` | Hit radius in game-space pixels |

### Touch Simulation

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `action_type` | `int` | `1` | Touch action: `1` = TOUCH_DOWN, `2` = TOUCH_UP, `4` = TOUCH_MOVED |
| `touch_id` | `int` | `0` | Unique touch pointer ID. The game engine uses this to track which "finger" is which. Must be unique per button |

### Keyboard Mapping

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `sdl_scancode` | `int` | `0` | Primary keyboard key (`SDL_Scancode` value) |
| `sdl_scancode_alt` | `int` | `0` | Secondary keyboard key. Either key activates the button |

### Gamepad Mapping

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `gamepad_button` | `int` | `-1` | `SDL_GamepadButton` value (`-1` = none) |
| `gamepad_axis` | `int` | `-1` | `SDL_GamepadAxis` value (`-1` = none) |
| `axis_threshold` | `float` | `0.5` | Minimum absolute axis value to activate (0.0–1.0) |

### TV Remote Mapping

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `tv_remote_key` | `int` | `-1` | Virtual key code for TV remote input (`-1` = none) |

### Macro Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `is_macro` | `bool` | `false` | If `true`, this button executes a multi-step touch sequence instead of a simple tap |
| `macro_open_touch_id` | `int` | `-1` | `touch_id` of the button to tap first (the "opener" — e.g., the item/magic menu button) |
| `macro_delay_ms` | `int` | `250` | Delay in milliseconds between the opener tap and the target tap |
| `macro_target_x` | `float` | `0` | Target X position for step 2 (the spell/item slot) |
| `macro_target_y` | `float` | `0` | Target Y position for step 2 |

### Runtime State (not serialized)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `is_pressed` | `bool` | `false` | Currently held down |
| `macro_pending` | `bool` | `false` | Macro sequence in progress |
| `macro_stage` | `int` | `0` | `0` = idle, `1` = opener pressed, `2` = opener released / waiting for delay, `3` = done |
| `macro_fire_time` | `uint64_t` | `0` | Timestamp (ms) when the macro sequence started |

### Constructor Defaults

```cpp
TouchButton() : is_custom(false),
                game_x(0), game_y(0), radius(40), action_type(1), touch_id(0),
                sdl_scancode(0), sdl_scancode_alt(0), gamepad_button(-1),
                gamepad_axis(-1), axis_threshold(0.5f), tv_remote_key(-1),
                is_macro(false), macro_open_touch_id(-1), macro_delay_ms(250),
                macro_target_x(0), macro_target_y(0),
                is_pressed(false), macro_pending(false), macro_stage(0),
                macro_fire_time(0) {}
```

---

## EditorMode Enum

Controls the behavior of the F2 controls editor:

| Value | Name | Description |
|-------|------|-------------|
| `0` | `EDITOR_POSITION` | Drag buttons to reposition them on screen |
| `1` | `EDITOR_REBIND` | Click a button, then press a key to rebind it |
| `2` | `EDITOR_ADD` | Adding a new custom button (WIP) |

While in the editor, press **R** to toggle between `EDITOR_POSITION` and `EDITOR_REBIND` modes.

---

## InputConfig Class

### Construction

| Method | Description |
|--------|-------------|
| `InputConfig()` | Constructor — calls `load_defaults()` |
| `~InputConfig()` | Destructor |

### Load / Save

| Method | Signature | Description |
|--------|-----------|-------------|
| `load` | `void load(const std::string& path)` | Load button config from an INI file. Falls back to defaults if file is missing or empty |
| `save` | `void save(const std::string& path)` | Save all buttons (including custom) to INI format |
| `load_defaults` | `void load_defaults()` | Reset to the 12 built-in buttons with default positions and bindings |

### Editor State

| Method | Signature | Description |
|--------|-----------|-------------|
| `is_editing` | `bool is_editing() const` | Returns `true` if the F2 editor is open |
| `toggle_editor` | `void toggle_editor()` | Toggle editor on/off. When closing, resets mode to `EDITOR_POSITION` |
| `set_editing` | `void set_editing(bool e)` | Explicitly set editor state |
| `get_editor_mode` | `EditorMode get_editor_mode() const` | Current editor sub-mode |
| `set_editor_mode` | `void set_editor_mode(EditorMode m)` | Change editor sub-mode |

### Rendering

| Method | Signature | Description |
|--------|-----------|-------------|
| `render_touch_zones` | `void render_touch_zones(int win_w, int win_h, bool show_labels = false)` | Draw translucent touch zone circles over the game. Color-coded by category (blue = movement, green = action, orange = utility, purple = macro, yellow = custom). Brighter when `is_pressed` |
| `render_editor` | `void render_editor(int win_w, int win_h, int mouse_x, int mouse_y, bool mouse_pressed)` | Draw the full editor overlay: dark background, labeled buttons with key bindings, crosshairs, instruction bar, and blink highlight for rebind targets |

### Input Lookup

All lookup methods return a button index (`0`–`N-1`) or `-1` if no match.

| Method | Signature | Description |
|--------|-----------|-------------|
| `find_button_at` | `int find_button_at(float game_x, float game_y)` | Find the button whose circle contains the given game-space point (Euclidean distance ≤ radius) |
| `find_by_scancode` | `int find_by_scancode(int scancode)` | Find button matching either primary or alt scancode |
| `find_by_gamepad_button` | `int find_by_gamepad_button(int button)` | Find button matching a gamepad button |
| `find_by_gamepad_axis` | `int find_by_gamepad_axis(int axis, float value)` | Find button matching an axis if `|value| >= axis_threshold` |

### Button Access

| Method | Signature | Description |
|--------|-----------|-------------|
| `get_buttons` | `const std::vector<TouchButton>& get_buttons() const` | Read-only access to the full button list |
| `get_button` | `TouchButton* get_button(int index)` | Get mutable pointer to button at index. Returns `nullptr` for out-of-range |
| `button_count` | `int button_count() const` | Number of registered buttons |

### Editor Event Handlers

| Method | Signature | Description |
|--------|-----------|-------------|
| `editor_mouse_down` | `void editor_mouse_down(float game_x, float game_y)` | In `EDITOR_POSITION`: start dragging the button under the cursor. In `EDITOR_REBIND`: select the button for rebinding |
| `editor_mouse_move` | `void editor_mouse_move(float game_x, float game_y)` | Update position of the dragged button (clamped to game bounds) |
| `editor_mouse_up` | `void editor_mouse_up()` | Stop dragging |
| `editor_handle_key` | `bool editor_handle_key(int scancode)` | In `EDITOR_REBIND` mode: rebind the selected button to the pressed key. Returns `true` if consumed. F1–F12 keys are rejected to prevent conflicts |

### Custom Button Management

| Method | Signature | Description |
|--------|-----------|-------------|
| `add_custom_button` | `int add_custom_button(const std::string& display_name, float gx, float gy, int scancode)` | Create a new custom button. Auto-assigns `touch_id` starting from 40. Returns the new button's index |
| `remove_button` | `bool remove_button(int index)` | Delete a button by index. **Only custom buttons** can be removed; built-in buttons are protected |

### Macro Processing

| Method | Signature | Description |
|--------|-----------|-------------|
| `process_macros` | `void process_macros(double current_time, TouchCallback callback)` | Must be called every frame. Advances all pending macro sequences through their stages, invoking `callback` to send the synthetic touch events |

### Utility

| Method | Signature | Description |
|--------|-----------|-------------|
| `scancode_name` | `static std::string scancode_name(int scancode)` | Returns human-readable key name via `SDL_GetScancodeName()`. Returns `"-"` for scancode ≤ 0, or `"KeyN"` if SDL doesn't know the name |

---

## TouchCallback Typedef

```cpp
typedef std::function<void(int action, int id, double time,
                           float x, float y,
                           float ox, float oy, int tap)> TouchCallback;
```

| Parameter | Description |
|-----------|-------------|
| `action` | Touch action type: `1` = DOWN, `2` = UP, `4` = MOVED |
| `id` | Touch pointer ID (matches `touch_id` on the button) |
| `time` | Current time as `double` seconds |
| `x`, `y` | Touch position in game coordinates |
| `ox`, `oy` | Original touch-down position (same as x/y for taps) |
| `tap` | Tap count (`1` for single tap, `0` for release) |

---

## Private Members

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `buttons` | `std::vector<TouchButton>` | — | All registered buttons |
| `editing_mode` | `bool` | `false` | Whether the editor is active |
| `editor_mode` | `EditorMode` | `EDITOR_POSITION` | Current editor sub-mode |
| `dragging_button` | `int` | `-1` | Index of button being dragged |
| `drag_offset_x/y` | `float` | `0` | Offset between cursor and button center during drag |
| `rebind_target` | `int` | `-1` | Index of button being rebound |
| `next_custom_touch_id` | `int` | `40` | Next auto-assigned touch ID for custom buttons |

---

## controls.ini Format

The config file uses a simple INI-style format with one section per button:

```ini
# Swordigo Desktop - Input Configuration v2
# Positions are in 960x544 game coordinate space
# Scancodes use SDL_Scancode values
# Two keys per action: scancode (primary) + scancode_alt (secondary)
# Macro buttons: press macro_open_touch_id, wait macro_delay_ms, tap macro_target

[left]
display_name=Move Left
game_x=65
game_y=80
radius=55
action_type=1
touch_id=10
scancode=4
scancode_alt=80
gamepad_button=13
gamepad_axis=-1
axis_threshold=0.5
tv_remote_key=-1

[magic_bolt]
display_name=Magic Bolt
game_x=898
game_y=426
radius=30
action_type=1
touch_id=30
scancode=30
scancode_alt=0
gamepad_button=-1
gamepad_axis=-1
axis_threshold=0.5
tv_remote_key=-1
is_macro=1
macro_open_touch_id=15
macro_delay_ms=250
macro_target_x=898
macro_target_y=426

[custom_40]
display_name=My Custom Button
game_x=400
game_y=300
radius=35
action_type=1
touch_id=40
scancode=20
scancode_alt=0
gamepad_button=-1
gamepad_axis=-1
axis_threshold=0.5
tv_remote_key=-1
is_custom=1
```

### INI Keys Reference

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `game_x` | float | Yes | X position |
| `game_y` | float | Yes | Y position |
| `radius` | float | Yes | Hit radius |
| `action_type` | int | Yes | Touch action type |
| `touch_id` | int | Yes | Unique pointer ID |
| `scancode` | int | Yes | Primary SDL scancode |
| `scancode_alt` | int | Yes | Secondary SDL scancode |
| `gamepad_button` | int | Yes | Gamepad button (-1 = none) |
| `gamepad_axis` | int | Yes | Gamepad axis (-1 = none) |
| `axis_threshold` | float | Yes | Axis activation threshold |
| `tv_remote_key` | int | Yes | TV remote key (-1 = none) |
| `display_name` | string | No | Custom display name |
| `is_custom` | bool | No | `1` or `true` for custom buttons |
| `is_macro` | bool | No | `1` or `true` for macro buttons |
| `macro_open_touch_id` | int | No | Opener button's touch_id |
| `macro_delay_ms` | int | No | Delay between macro steps |
| `macro_target_x` | float | No | Step 2 target X |
| `macro_target_y` | float | No | Step 2 target Y |

Lines starting with `#` or `;` are comments. Whitespace around `=` is trimmed.

---

## Default Button Layout (960×544)

The 12 built-in buttons with their default positions and bindings:

### Movement Buttons (Left Side)

| Name | Position | Radius | Primary Key | Alt Key | Touch ID | Gamepad |
|------|----------|--------|-------------|---------|----------|---------|
| `left` | (65, 80) | 55 | A | ← | 10 | D-Pad Left |
| `right` | (195, 80) | 55 | D | → | 11 | D-Pad Right |

### Action Buttons (Right Side)

| Name | Position | Radius | Primary Key | Alt Key | Touch ID | Gamepad |
|------|----------|--------|-------------|---------|----------|---------|
| `jump` | (913, 132) | 45 | Space | W | 12 | South (A/×) |
| `attack` | (794, 110) | 40 | K | Z | 13 | West (X/□) |
| `magic` | (916, 210) | 35 | J | X | 14 | North (Y/△) |

### Utility Buttons

| Name | Position | Radius | Primary Key | Alt Key | Touch ID | Gamepad |
|------|----------|--------|-------------|---------|----------|---------|
| `use_item` | (910, 489) | 30 | I | — | 15 | East (B/○) |
| `menu` | (480, 30) | 25 | Escape | — | 16 | Start |
| `pause` | (55, 485) | 25 | P | — | 17 | Back |

### Magic Spell Macros

| Name | Position | Radius | Key | Touch ID | Opener | Delay | Display |
|------|----------|--------|-----|----------|--------|-------|---------|
| `magic_bolt` | (898, 426) | 30 | 1 | 30 | 15 (use_item) | 250ms | Magic Bolt |
| `magic_bomb` | (906, 329) | 30 | 2 | 31 | 15 (use_item) | 250ms | Bomb |
| `magic_dragon` | (889, 243) | 30 | 3 | 32 | 15 (use_item) | 250ms | Dragon Gasp |
| `magic_rift` | (891, 105) | 30 | 4 | 33 | 15 (use_item) | 250ms | Dimensional Rift |

### Visual Layout Map

```
     (480,30)
       [Menu]
                                                      (891,105)
                                                      ⚡Rift [4]

                                                      (889,243)
                                                      ⚡Dragon [3]

                                                   (794,110)    (913,132)
(65,80)    (195,80)                                [Attack K]    [Jump Space]
[Left A]   [Right D]                                          (916,210)
                                                              [Magic J]
                                                      (906,329)
                                                      ⚡Bomb [2]

                                                      (898,426)
                                                      ⚡Bolt [1]

(55,485)                                                      (910,489)
[Pause P]                                                     [Item I]
```

---

## Macro System

Macros solve a desktop-specific problem: the game's magic spell menu requires a precise multi-step touch sequence that's trivial on a touchscreen but awkward with a keyboard.

### How It Works

1. **User presses a macro key** (e.g., `1` for Magic Bolt)
2. **Stage 1** — The system simulates a touch-down on the opener button (the item menu, touch_id `15`). This opens the spell selection wheel.
3. **Stage 1→2** — After 50ms, the opener touch is released (completing the tap). The menu begins opening.
4. **Stage 2→Done** — After `macro_delay_ms` (250ms default), the system simulates a tap at the macro target position — the spell slot's coordinates. This selects the spell.
5. **Stage 3** — After an additional 50ms, the slot tap is released. The macro sequence is complete.

### Timing Diagram

```
t=0ms     : Key pressed → opener TOUCH_DOWN at use_item position
t=50ms    : Opener TOUCH_UP (tap complete, menu opens)
t=250ms   : Spell slot TOUCH_DOWN at macro_target position
t=300ms   : Spell slot TOUCH_UP (selection complete)
```

### Stage Machine

| Stage | Condition | Action |
|-------|-----------|--------|
| `0` | Idle | Waiting for key press |
| `1` | `elapsed >= 50ms` | Release opener (TOUCH_UP) → stage 2 |
| `2` | `elapsed >= macro_delay_ms` | Tap spell slot (TOUCH_DOWN) → stage 3 |
| `3` | `elapsed >= macro_delay_ms + 50ms` | Release spell slot (TOUCH_UP) → stage 0 |

### Customizing Macros

The default macro targets are positioned at the spell slot locations in the game's magic menu. If you reposition the macro buttons using the F2 editor, the `macro_target_x/y` values are automatically updated (they track the button's game_x/y on save).

You can also create custom macros by manually editing `controls.ini`:

```ini
[custom_potion]
display_name=Quick Potion
game_x=800
game_y=400
radius=30
touch_id=41
scancode=24
is_macro=1
macro_open_touch_id=15
macro_delay_ms=300
macro_target_x=800
macro_target_y=400
is_custom=1
```

---

## Render Color Coding

Buttons are color-coded in both the touch zone overlay and the editor:

| Category | Color | Example Buttons |
|----------|-------|-----------------|
| Movement | Blue (50, 120, 255) | `left`, `right` |
| Utility | Orange (255, 160, 40) | `menu`, `pause` |
| Macro | Purple (180, 80, 220) | `magic_bolt`, `magic_bomb`, etc. |
| Custom | Yellow (220, 220, 60) | Any user-created button |
| Action | Green (60, 220, 80) | `jump`, `attack`, `magic`, `use_item` |

---

## Key Rebinding Logic

When in `EDITOR_REBIND` mode and a key is pressed:

1. **F1–F12 are rejected** — these are system shortcuts and cannot be rebound
2. **If the key is already the primary** — no change (log message only)
3. **If the alt slot is empty or already this key** — assign as alt key
4. **Otherwise** — the pressed key becomes the new primary, and the old primary shifts to the alt slot

This "rotate in" behavior ensures you never accidentally lose a binding.

---

## Integration Points

- **F2** — Toggle controls editor (saves on close)
- **R** (while editor open) — Toggle between Position and Rebind modes
- Config file auto-saved to `save/controls_arm64.ini` when editor is closed
- Loaded at game startup; falls back to defaults if missing
