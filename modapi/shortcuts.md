# Keyboard Shortcuts Reference

Complete list of keyboard shortcuts available in Swordigo Desktop.

---

## Function Keys

| Key | Action | Context |
|-----|--------|---------|
| **F1** | Toggle GUI overlay (menu bar) | In-game |
| **F2** | Toggle controls editor | In-game |
| **F3** | Toggle debug overlay | In-game |
| **F4** | Cycle FBO scale mode | In-game |
| **F5** | Toggle free camera | In-game |
| **F6** | Cycle PostFX presets | In-game |
| **F7** | Toggle typing mode | In-game |
| **F10** | Toggle SRT overlay (inventory editor) | In-game |
| **F12** | Toggle fullscreen | Anywhere |

---

## FBO Scale Modes (F4 Cycle)

Each press of F4 cycles through:

1. **Sharp Bilinear** (default) — smooth upscaling with sharp edges
2. **Nearest** — pixel-perfect, no interpolation
3. **CRT Scanline** — retro CRT effect with scanlines
4. **FSR** — AMD FidelityFX Super Resolution 1.0

---

## PostFX Presets (F6 Cycle)

Each press of F6 cycles through:

1. **Off** — no post-processing
2. **Swordigo Plus** — subtle enhancements (vignette, slight color adjustment)
3. **Atmospheric** — god rays, volumetric light, warm tones
4. **Ethereal** — bloom, chromatic aberration, cool tones
5. **Cinematic** — film grain, vignette, color grading
6. **Retro** — CRT-style effects, desaturation
7. **Fantasy** — vivid colors, bloom, outlines
8. **Noir** — black and white, high contrast, film grain
9. **Custom** — user-defined settings (configured in overlay)

---

## Game Speed Keys

| Key | Speed | Description |
|-----|-------|-------------|
| **1** | 1.0x | Normal speed |
| **2** | 2.0x | Double speed |
| **3** | 4.0x | Quad speed |
| **4** | 0.5x | Half speed (slow-motion) |

---

## Free Camera (F5)

When free camera is active:

| Key | Action |
|-----|--------|
| **W** | Move forward |
| **A** | Move left |
| **S** | Move backward |
| **D** | Move right |
| **Arrow Keys** | Pan camera |
| **Mouse Scroll** | Zoom in/out |

---

## Launcher Shortcuts

| Key | Action |
|-----|--------|
| **Enter** | Launch selected instance |
| **Escape** | Close launcher without launching |
| **Delete** | Remove selected custom instance (with confirmation) |

---

## SRT Overlay (F10)

When the SRT overlay is open:

| Key | Action |
|-----|--------|
| **Ctrl+S** | Quick save current inventory edits |
| **Tab** | Cycle between panels (Inventory / Stats / Items) |
| **Escape** | Close overlay |

---

## Debug Overlay (F3) Info

The debug overlay displays (read-only):

- FPS counter (green)
- Frame counter + architecture tag
- Draw calls / texture binds
- Vertices / matrix ops
- Render resolution → Window resolution
- Mouse position + delta time
- Scale mode / PostFX preset
- Game speed / pause state
- Camera debug (position, yaw, pitch, distance)
- Binary info (filename + label)
- Graphics API (OpenGL / Vulkan)
- Player stats (HP, Mana, Coins, Level, XP, ATK — color-coded)

---

## Related Documentation

- [Debug Overlay](debug-overlay.md) — detailed F3 overlay reference
- [PostFX System](postfx.md) — PostFX presets and configuration
- [FBO Scaler](fbo-scaler.md) — scaling modes
- [Input System](input-system.md) — keybinding configuration
- [Overlay System](overlay.md) — SRT runtime overlay
