# F3 Debug Overlay

The debug overlay is an in-game heads-up display that shows real-time runtime
statistics, rendering metrics, player stats, and system information. It is rendered
on top of everything else (after PostFX) using the host's OpenGL context.

**Source:** [main.cpp:3523–3627](file:///home/quantumcreeper/SwordigoDesktop/src/main.cpp#L3523-L3627)
**Toggle:** Press **F3** during gameplay (non-repeating — hold does not toggle repeatedly)

---

## Visual Layout

The overlay renders as a dark semi-transparent panel in the **top-left corner** of the
window. All text is drawn using `GuiRenderer::draw_string()`.

```
┌──────────────────────────────────────────┐
│  FPS: 60.0                      (green)  │
│  Frame: 12847  [ARM64]          (white)  │
│  Draws: 142  TexBinds: 38      (grey)   │
│  Verts: 8420  MatOps: 312      (grey)   │
│  State: 24  TexUps: 6          (grey)   │
│  Render: 1024x576 → Window: 1920x1080   │
│    (Draw: 1920x1080)         (dim grey)  │
│  Mouse: 540,380  DT: 0.0167s (dim grey) │
│  Scale Mode: Sharp-Bilinear    (gold)    │
│  Speed: 1.0x  || PAUSED       (orange)  │
│  F1:GUI F2:Ctrl F3:Dbg F4:Scale ...     │
│  TYPING MODE ACTIVE             (red)    │
│  Cam: pos(12.3,4.5,8.0) yaw:45 pitch:20 │
│  Binary: libswordigo.so (v1.4.12 ARM64)  │
│  PostFX: CRT-Warm              (orange)  │
│  Graphics API: OpenGL           (cyan)   │
│  HP: 8/12  Mana: 40/70  Coins: 342      │
│  Lv.5  XP: 1200  ATK: 2       (purple)  │
└──────────────────────────────────────────┘
```

---

## Displayed Information

### Line-by-Line Reference

| # | Content | Format | Color | Source |
|---|---|---|---|---|
| 1 | **FPS** | `"FPS: %.1f"` | Green `(0, 255, 100)` | `fps` (smoothed frame rate) |
| 2 | **Frame count** | `"Frame: %d  [ARM64]"` | White `(200, 200, 200)` | `completed_frames` counter |
| 3 | **Draw calls + Texture binds** | `"Draws: %d  TexBinds: %d"` | Grey `(180, 180, 180)` | `g_frame_stats` |
| 4 | **Vertices + Matrix ops** | `"Verts: %d  MatOps: %d"` | Grey `(180, 180, 180)` | `g_frame_stats` |
| 5 | **State changes + Texture uploads** | `"State: %d  TexUps: %d"` | Grey `(180, 180, 180)` | `g_frame_stats` |
| 6 | **Resolution chain** | `"Render: %dx%d -> Window: %dx%d (Draw: %dx%d)"` | Dim grey `(140, 140, 160)` | `GAME_W/H`, `g_win_w/h`, `g_draw_w/h` |
| 7 | **Mouse + Delta time** | `"Mouse: %d,%d  DT: %.4fs"` | Dim grey `(140, 140, 160)` | `mouse_x/y`, `dt_seconds` |
| 8 | **Scale mode** | `"Scale Mode: %s"` | Gold `(255, 200, 100)` | `g_fbo_mode` enum |
| 9 | **Game speed + Pause** | `"Speed: %s  || PAUSED"` | Orange `(255, 180, 50)` | `mod_speed_label()`, `g_game_paused` |
| 10 | **Keyboard shortcuts** | `"F1:GUI F2:Ctrl F3:Dbg F4:Scale F5:Cam F6:PostFX F7:Type F10:HUD"` | Dark grey `(100, 100, 120)` | Static string |
| 11 | **Typing mode** | `"TYPING MODE ACTIVE"` | Red `(255, 100, 100)` | `g_typing_mode` flag |
| 12 | **Camera debug** | Position, yaw, pitch, distance | Green/grey (contextual) | `cam_debug_string()` |
| 13 | **Binary info** | `"Binary: %s (%s)"` | Cyan `(100, 200, 255)` | `g_binary_selector` or `g_lib_name` |
| 14 | **PostFX preset** | `"PostFX: %s"` or `"PostFX: Off"` | Orange / dim grey | `g_postfx.preset_name`, `.enabled` |
| 15 | **Graphics API** | `"Graphics API: %s"` | Light cyan `(100, 220, 255)` | `g_graphics_api` enum |
| 16 | **Player HP/Mana/Coins** | `"HP: %d/%d  Mana: %d/%d  Coins: %d"` | Health-coded (see below) | SRE shared globals |
| 17 | **Level/XP/ATK** | `"Lv.%d  XP: %d  ATK: %d"` | Purple `(180, 140, 255)` | SRE shared globals |

---

## Detailed Sections

### Performance Metrics (Lines 1–5)

The FPS counter uses a smoothed average. The `[ARM64]` tag on line 2 indicates the
emulated architecture.

Frame statistics are accumulated per-frame by the JNI bridge's OpenGL wrapper and
reset at the start of each frame via `g_frame_stats.reset()`.

| Metric | Description |
|---|---|
| `draw_calls` | Number of `glDrawArrays` / `glDrawElements` calls |
| `texture_binds` | Number of `glBindTexture` calls |
| `vertices_submitted` | Total vertex count across all draw calls |
| `matrix_ops` | `glPushMatrix`, `glPopMatrix`, `glMultMatrix`, etc. |
| `state_changes` | `glEnable`, `glDisable`, `glBlendFunc`, etc. |
| `tex_uploads` | `glTexImage2D`, `glTexSubImage2D` calls |

### Resolution Chain (Line 6)

Shows the full rendering pipeline resolution:

```
Render: 1024x576 → Window: 1920x1080 (Draw: 1920x1080)
  │                  │                  │
  │                  │                  └─ Actual framebuffer size (g_draw_w × g_draw_h)
  │                  └─ SDL window size (g_win_w × g_win_h)
  └─ Game's internal render target (GAME_W × GAME_H = 1024×576)
```

The game renders at its native resolution into an FBO, which is then upscaled to the
window size using the selected scale mode.

### Scale Mode (Line 8)

Displays the current FBO upscaling algorithm. Cycled with **F4**:

| Mode | Description |
|---|---|
| `Sharp-Bilinear` | Default — sharp pixel edges with bilinear interpolation |
| `Nearest` | Nearest-neighbor — pure pixel-perfect scaling |
| `CRT Scanline` | CRT emulation with scanline darkening |
| `FSR 1.0` | AMD FidelityFX Super Resolution — edge-preserving upscale |

### Game Speed (Line 9)

Shows the current time scale multiplier and pause state:

- `mod_speed_label()` returns a string like `"1.0x"`, `"0.5x"`, `"2.0x"`
- If `g_game_paused` is true, `"|| PAUSED"` is appended

### Camera Debug (Line 12)

`cam_debug_string()` produces a compact debug string with:
- Camera world position (X, Y, Z)
- Yaw and pitch angles
- Camera distance from target

Color changes based on `g_cam_active`:
- **Active (free cam):** Green `(0, 220, 100)`
- **Inactive (game cam):** Grey `(120, 120, 120)`

### Binary Info (Line 13)

Shows the loaded ARM64 binary:
- If managed by the binary selector: `"Binary: libswordigo.so (v1.4.12 ARM64)"`
- Fallback: `"Binary: libswordigo.so"` (raw filename)

### Typing Mode (Line 11)

When `g_typing_mode` is active, keyboard input goes to the game's text fields instead
of being interpreted as shortcut keys. Shown in **red** as a warning.

---

## Player Stats (Lines 16–17)

> [!IMPORTANT]
> Player stats only appear when a game scene is active. They are **not** shown on the
> main menu or loading screens.

### Data Source

Stats are read every frame from **SRE shared globals** in guest memory. The
`sre_GameSceneView_Update` hook in [sre_scene_update.c](file:///home/quantumcreeper/SwordigoDesktop/src/sre/sre_scene_update.c)
extracts player data from the engine's `GameState` struct and writes it to
`volatile int` globals.

### SRE Shared Globals

These globals are declared in `sre_scene_update.c` and their guest virtual addresses
are resolved at startup by the host:

| Guest Global | Host Address Variable | Type | Description |
|---|---|---|---|
| `g_sre_player_hp` | `sre_player_hp_addr` | `volatile int` | Current health points |
| `g_sre_player_max_hp` | `sre_player_max_hp_addr` | `volatile int` | Max HP (computed: `level * 2 + 4`) |
| `g_sre_player_mana` | `sre_player_mana_addr` | `volatile int` | Current mana |
| `g_sre_player_max_mana` | `sre_player_max_mana_addr` | `volatile int` | Max mana (computed: `level * 20 + 10`) |
| `g_sre_player_coins` | `sre_player_coins_addr` | `volatile int` | Coin count |
| `g_sre_player_xp` | `sre_player_xp_addr` | `volatile int` | Experience points |
| `g_sre_player_level` | `sre_player_level_addr` | `volatile int` | Experience level |
| `g_sre_player_atk_level` | `sre_player_atk_level_addr` | `volatile int` | Attack attribute level |
| `g_sre_gui_scene_active` | `sre_gui_scene_active_addr` | `volatile int` | `1` when game scene is active |

### Health Color Coding

The HP/Mana/Coins line uses dynamic coloring based on health percentage:

```c
int hp_pct = max_hp > 0 ? (hp * 100 / max_hp) : 0;
uint8_t hr = hp_pct > 50 ? 80 : 255;    // Red channel
uint8_t hg = hp_pct > 25 ? 255 : 80;    // Green channel
// Blue is always 80
```

| Health % | Color | Visual |
|---|---|---|
| > 50% | Green `(80, 255, 80)` | Healthy |
| 26–50% | Yellow `(255, 255, 80)` | Caution |
| ≤ 25% | Red `(255, 80, 80)` | Critical |

---

## Rendering Architecture

### OpenGL State

The overlay sets up its own orthographic projection and saves/restores all OpenGL
state using `glPushAttrib(GL_ALL_ATTRIB_BITS)`:

```c
glPushAttrib(GL_ALL_ATTRIB_BITS);
glViewport(0, 0, g_draw_w, g_draw_h);
glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
glOrtho(0, g_win_w, 0, g_win_h, -1, 1);
glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
glDisable(GL_TEXTURE_2D);
glDisable(GL_LIGHTING);
glDisable(GL_DEPTH_TEST);
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

### Background Quad

A dark semi-transparent quad is drawn behind all text for readability:

```c
glColor4ub(0, 0, 0, 180);  // Black, ~70% opacity
glBegin(GL_QUADS);
glVertex2f(10, g_win_h - 10);       // Top-left
glVertex2f(420, g_win_h - 10);      // Top-right
glVertex2f(420, g_win_h - 315);     // Bottom-right
glVertex2f(10, g_win_h - 315);      // Bottom-left
glEnd();
```

Panel dimensions: **410 × 305 pixels**, positioned 10px from the top-left corner.

### Text Rendering

All text is rendered via `GuiRenderer::draw_string()`:

```cpp
void draw_string(const std::string& str, float x, float y,
                 float scale, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
```

| Parameter | Description |
|---|---|
| `str` | Text to render |
| `x`, `y` | Position in window coordinates (origin = bottom-left) |
| `scale` | Text size multiplier (FPS uses `2.0`, most lines use `1.2`) |
| `r, g, b, a` | RGBA color (0–255 each) |

### Render Timing

The overlay renders **after** PostFX but **before** the SDL buffer swap:

```
1. Game renders to FBO (Unicorn emulation)
2. FBO scaled to window (selected scale mode)
3. PostFX applied (if enabled)
4. F3 debug overlay rendered          ← HERE
5. Mod tools overlay (speed, toasts)
6. SRT overlay (F9 inventory editor)
7. SDL_GL_SwapWindow()
```

---

## Keyboard Shortcut Reference

The overlay displays a compact shortcut reference on line 10. Full shortcut details:

| Key | Function | Description |
|---|---|---|
| **F1** | GUI | Toggle GUI visibility |
| **F2** | Ctrl | Toggle touch controls overlay |
| **F3** | Dbg | Toggle this debug overlay |
| **F4** | Scale | Cycle FBO scale mode |
| **F5** | Cam | Toggle free camera |
| **F6** | PostFX | Cycle PostFX presets |
| **F7** | Type | Toggle typing mode |
| **F10** | HUD | Toggle game HUD |

> [!TIP]
> See the full [Keyboard Shortcuts](shortcuts.md) reference for all keybindings
> including game speed controls, camera movement, and save/load.

---

## Conditional Display

| Condition | Lines Shown/Hidden |
|---|---|
| `debug_visible == false` | Entire overlay hidden |
| `g_display_active == false` | Entire overlay hidden (no active display) |
| `g_typing_mode == true` | "TYPING MODE ACTIVE" line appears |
| `g_postfx.enabled == false` | PostFX line shows "Off" in dim grey |
| Scene not active | Player stats lines (16–17) hidden |
| No binary info loaded | Falls back to raw `g_lib_name` string |

---

## Related Documentation

- [FBO Scaler](fbo-scaler.md) — Resolution chain and scaling modes
- [PostFX System](postfx.md) — Post-processing presets
- [SRE Hook API](sre-hooks.md) — GameSceneView::Update hook (stat extraction)
- [Overlay System](overlay.md) — F9 inventory editor overlay
- [Keyboard Shortcuts](shortcuts.md) — Full keybinding reference
- [Input System](input-system.md) — Typing mode and input routing
