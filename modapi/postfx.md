# Post-Processing Effects (PostFX)

> **Header**: [`src/platform/fbo_scaler.h`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/fbo_scaler.h) (lines 19–88)
> **Source**: [`src/platform/fbo_scaler.cpp`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/fbo_scaler.cpp) (lines 1598–1709)
> **Cycle presets**: **F6**

The PostFX system applies real-time post-processing effects to the game's rendered output. Effects are organized into two tiers: lightweight color effects (Tier 1) and advanced GPU-intensive effects (Tier 2). All effects are applied as GLSL shader passes between the game FBO and the final window blit.

---

## Architecture

```
Game renders to 960×544 FBO
    ↓
┌─ Tier 2 passes (separate FBOs) ──────────────────┐
│  SSAO → blur → half-res AO texture                │
│  God Rays → half-res rays texture                  │
│  Bloom Extract → quarter-res → blur → bloom tex    │
│  Composite: scene + AO + rays + bloom + shadows    │
└───────────────────────────────────────────────────┘
    ↓
┌─ Tier 1 pass (single full-res shader) ────────────┐
│  Chromatic aberration → Sharpening → Brightness    │
│  → Contrast → Saturation → Warmth → Film grain    │
│  → Vignette → Edge outlines                       │
└───────────────────────────────────────────────────┘
    ↓
Upscale to window (Sharp-Bilinear / Nearest / CRT / FSR)
```

---

## PostFXState Struct

The complete state of all post-processing effects. Passed to `fbo_end_game_and_blit()` to control the rendering pipeline.

### Master Control

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | `bool` | `false` | Master switch — when `false`, all PostFX passes are skipped |
| `preset_name` | `const char*` | `"Off"` | Display name of the active preset (set by `postfx_apply_preset`) |

### Tier 1: Color Effects (Single-Pass Shader)

These effects are combined into one GLSL fragment shader for maximum performance.

#### Vignette

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `vignette` | `bool` | `false` | Enable darkening at screen edges |
| `vignette_intensity` | `float` | `0.3` | Strength of the darkening (0.0 = none, 1.0 = heavy) |

#### Film Grain

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `film_grain` | `bool` | `false` | Enable random noise overlay |
| `grain_intensity` | `float` | `0.06` | Noise amplitude (0.0–0.15 recommended) |

#### Chromatic Aberration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `chromatic_aberration` | `bool` | `false` | Enable RGB channel separation at screen edges |
| `ca_offset` | `float` | `0.002` | Separation distance in UV space (0.001 = subtle, 0.005 = extreme) |

#### Color Adjustment

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `color_adjust` | `bool` | `false` | Enable color grading |
| `saturation` | `float` | `1.0` | Color intensity multiplier (0.0 = grayscale, 1.0 = normal, 1.5 = vivid) |
| `contrast` | `float` | `1.0` | Contrast multiplier (1.0 = normal, 1.5 = punchy) |
| `brightness` | `float` | `0.0` | Additive brightness offset (-0.2 to +0.2 typical) |
| `warmth` | `float` | `0.0` | Warm/cool shift: positive = warm (adds red, removes blue), negative = cool |

> [!TIP]
> Warmth is applied as `R += warmth * 0.05`, `B -= warmth * 0.05`. Values around ±0.3 give a noticeable but natural tint.

#### Sharpen

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `sharpen` | `bool` | `false` | Enable unsharp masking |
| `sharpen_strength` | `float` | `0.4` | Sharpening intensity (0.2 = subtle, 0.8 = aggressive) |

### Tier 2: Advanced Effects (Multi-Pass)

These effects use additional framebuffers and multiple shader passes. They have a higher GPU cost but create dramatic visual improvements.

#### God Rays

Screen-space radial blur emanating from a configurable sun position. Uses 64 samples per pixel with depth-aware occlusion masking — foreground objects block light, only sky/background emits rays.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `god_rays` | `bool` | `false` | Enable volumetric light shafts |
| `god_rays_intensity` | `float` | `0.4` | Overall ray brightness (0.0–1.0) |
| `god_rays_decay` | `float` | `0.96` | Per-sample falloff (0.90 = short rays, 0.99 = long rays) |
| `sun_x` | `float` | `0.5` | Sun position X in UV space (0.0 = left, 1.0 = right) |
| `sun_y` | `float` | `0.85` | Sun position Y in UV space (0.0 = bottom, 1.0 = top) |

> [!NOTE]
> God rays use depth masking: only pixels with depth > 0.95 (sky/far background) emit light. Close objects cast shadows by blocking the ray march.

#### Screen-Space Ambient Occlusion (SSAO)

16-sample hemisphere kernel with per-pixel noise rotation. Rendered at half resolution and Gaussian-blurred for smooth results.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `ssao` | `bool` | `false` | Enable ambient occlusion |
| `ssao_radius` | `float` | `0.02` | Sample radius in UV space (larger = wider darkening) |
| `ssao_intensity` | `float` | `1.2` | Darkening strength (0.0 = none, 2.0 = very dark corners) |

#### Volumetric Light

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `volumetric_light` | `bool` | `false` | Enable atmospheric light scattering |
| `volumetric_intensity` | `float` | `0.3` | Scattering strength |

#### Bloom

Extracts bright areas of the image (above a luminance threshold), blurs them at quarter resolution, and adds the result back to create a soft glow.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `bloom` | `bool` | `false` | Enable bloom glow |
| `bloom_threshold` | `float` | `0.7` | Minimum brightness for bloom extraction (0.0 = everything blooms, 1.0 = only pure white) |
| `bloom_intensity` | `float` | `0.35` | Additive bloom strength |

#### Screen-Space Shadows

Directional shadow mapping via depth ray marching. Marches 12 steps from each pixel toward the light source, checking for depth occlusion.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `shadows` | `bool` | `false` | Enable directional shadows |
| `shadow_intensity` | `float` | `0.5` | Shadow darkness (0.0 = none, 1.0 = pitch black) |
| `shadow_softness` | `float` | `0.003` | Blur radius for soft shadow edges (UV-space step size) |
| `shadow_light_x` | `float` | `0.3` | Light direction X (+right, normalized) |
| `shadow_light_y` | `float` | `-0.8` | Light direction Y (-down, normalized) |

> [!NOTE]
> Shadows skip far background pixels (linearized depth > 0.95) and only register occluders within a depth difference of 0.001–0.15 to prevent distant walls from casting false shadows.

#### Edge Outlines

Depth-based edge detection using a Sobel-like 3×3 cross pattern. Creates cel-shading / illustration outlines on depth discontinuities, with secondary color-edge detection for objects at the same depth. Outlines are rendered as **color-aware darkened shades** — not black lines.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `outlines` | `bool` | `false` | Enable depth-based edge outlines |
| `outline_thickness` | `float` | `1.0` | Pixel width of outline (1.0 = 1px, 2.0 = thicker) |
| `outline_intensity` | `float` | `0.7` | Outline opacity (0.0–1.0) |
| `outline_depth_threshold` | `float` | `0.002` | Depth difference required to detect an edge |

> [!TIP]
> The outline system samples the average color of neighboring pixels and creates a darkened, slightly more saturated version for the outline tint. This makes outlines feel like natural contour shadows rather than drawn lines.

---

## PostFXPreset Enum

Presets are cycled with **F6**. Each preset configures a specific combination of effects.

```cpp
enum class PostFXPreset : int {
    OFF = 0,
    SW_PLUS,        // Swordigo Plus
    ATMOSPHERIC,
    ETHEREAL,
    CINEMATIC,
    RETRO,
    FANTASY,
    NOIR,
    CUSTOM,
    COUNT           // Sentinel (used for cycling)
};
```

### Preset Configurations

| Preset | Display Name | Key Effects | Character |
|--------|-------------|-------------|-----------|
| `OFF` | Off | None | Vanilla rendering |
| `SW_PLUS` | Sw+ | SSAO + God Rays + Bloom + Shadows + Outlines + Warm Color + Vignette + CA + Sharpen | The "premium" preset — everything turned on, carefully balanced |
| `ATMOSPHERIC` | Atmospheric | SSAO + Volumetric + Bloom + Shadows + Vignette | Dark, moody atmosphere with strong ambient occlusion |
| `ETHEREAL` | Ethereal | God Rays + Bloom + Warm Color + Vignette | Soft golden glow with heavenly light shafts |
| `CINEMATIC` | Cinematic | Vignette + Film Grain + Warm Color + Bloom | Movie-like with grain and warm tones |
| `RETRO` | Retro | Film Grain + High Saturation + Strong CA | Retro oversaturated look with chromatic aberration |
| `FANTASY` | Fantasy | Warm Color + Bloom + Bold Outlines + Sharpen + Vignette | Storybook illustration style with thick outlines |
| `NOIR` | Noir | Heavy Vignette + Grain + Near-Grayscale + High Contrast | Black-and-white film noir aesthetic |
| `CUSTOM` | Custom | User-defined | Placeholder for manual configuration |

### Sw+ (Swordigo Plus) Detailed Settings

The flagship preset uses these exact values:

```cpp
// SSAO
ssao = true; ssao_radius = 0.025; ssao_intensity = 1.2;
// God Rays
god_rays = true; god_rays_intensity = 0.6; god_rays_decay = 0.96;
sun_x = 0.65; sun_y = 0.88;
// Volumetric
volumetric_light = true; volumetric_intensity = 0.3;
// Color
color_adjust = true; warmth = 0.2; contrast = 1.05;
saturation = 1.12; brightness = 0.03;
// Vignette
vignette = true; vignette_intensity = 0.2;
// Sharpen
sharpen = true; sharpen_strength = 0.3;
// Chromatic Aberration
chromatic_aberration = true; ca_offset = 0.0008;
// Bloom
bloom = true; bloom_threshold = 0.65; bloom_intensity = 0.25;
// Shadows
shadows = true; shadow_intensity = 0.45; shadow_softness = 0.003;
shadow_light_x = 0.3; shadow_light_y = -0.8;
// Outlines
outlines = true; outline_thickness = 1.0; outline_intensity = 0.5;
outline_depth_threshold = 0.002;
```

---

## Functions

### postfx_apply_preset

```cpp
void postfx_apply_preset(PostFXState& state, PostFXPreset preset);
```

Resets the `PostFXState` to default (`PostFXState{}`) and then applies the preset's configuration. This means **all** effects are reset before the preset values are set — there is no additive layering.

### postfx_preset_name

```cpp
const char* postfx_preset_name(PostFXPreset p);
```

Returns the display name string for a preset. Used for HUD display and console logging.

| Preset | Return Value |
|--------|-------------|
| `OFF` | `"Off"` |
| `SW_PLUS` | `"Sw+"` |
| `ATMOSPHERIC` | `"Atmospheric"` |
| `ETHEREAL` | `"Ethereal"` |
| `CINEMATIC` | `"Cinematic"` |
| `RETRO` | `"Retro"` |
| `FANTASY` | `"Fantasy"` |
| `NOIR` | `"Noir"` |
| `CUSTOM` | `"Custom"` |

---

## Shader Implementation Notes

### Tier 1 PostFX Shader Pipeline

All Tier 1 effects are combined in a single fragment shader (`FRAG_POSTFX`) for efficiency:

1. **Chromatic aberration** — Samples R, G, B channels at offset UVs along a radial direction from screen center
2. **Sharpening** — Unsharp mask: `result = color + (color - blur) * strength`
3. **Brightness** — Additive: `color += brightness`
4. **Contrast** — `color = (color - 0.5) * contrast + 0.5`
5. **Saturation** — Lerp between luminance and color: `mix(vec3(luma), color, saturation)`
6. **Warmth** — Simple channel shift: `R += warmth * 0.05; B -= warmth * 0.05`
7. **Film grain** — Random noise based on UV + time
8. **Vignette** — `color *= pow(vig.x * vig.y * 15.0, intensity)` where `vig = uv * (1 - uv)`
9. **Edge outlines** — Depth-based Sobel edge detection with color-aware darkening

### Tier 2 Composite Shader

The composite pass (`FRAG_COMPOSITE`) merges all advanced effect textures:

1. Apply SSAO (multiply scene by AO value, with shadow lift)
2. Apply screen-space shadows (12-step depth ray march)
3. Apply ambient sky color fill (subtle tint in dark areas)
4. Add god rays (additive with color tint)
5. Add bloom (additive glow)
6. Extended Reinhard tone mapping: `scene * (1 + scene / Lwhite²) / (1 + scene)`
7. Saturation boost (×1.15)
8. Contrast micro-curve: `smoothstep(0, 1, scene)`

### FBO Resources

The PostFX pipeline uses these GPU resources:

| FBO | Resolution | Purpose |
|-----|-----------|---------|
| Main game FBO | 960×544 | Color + depth texture |
| PostFX FBO A | 960×544 | Full-res ping buffer |
| PostFX FBO B | 960×544 | Full-res pong buffer |
| Half-res FBO A/B | 480×272 | SSAO render + blur |
| Bloom FBO A/B | 240×136 | Bloom extract + blur |

---

## Time of Day System

The **N** key cycles through three time-of-day modes (Day → Afternoon → Night), each of which modifies the active PostFX preset in-place:

| Mode | Warmth | Brightness | Saturation | God Rays | SSAO | Notes |
|------|--------|------------|------------|----------|------|-------|
| Day | (preset default) | (preset default) | (preset default) | (preset default) | (preset default) | Vanilla preset, BG brightness = 1.0 |
| Afternoon | +0.35 | +0.05 | 1.2 | Intense (0.8) | Strong (1.5) | Golden hour, sun at (0.75, 0.7), BG brightness = 1.1 |
| Night | −0.25 | −0.12 | 0.6 | Off | Strong (1.6) | Cool blue, heavy vignette (0.55), grain, BG brightness = 0.3 |

---

## Menu Detection

The PostFX system automatically suppresses effects during full-screen menus to prevent visual artifacts. Detection uses GL state tracking:

- `g_frame_has_ortho > 0 && g_frame_has_perspective == 0` → game is in a 2D menu (disable PostFX)
- When the menu closes and perspective rendering resumes, the user's preset is restored
