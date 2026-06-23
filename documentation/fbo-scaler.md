# FBO Scaler — Rendering Pipeline

> **Header**: [`src/platform/fbo_scaler.h`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/fbo_scaler.h)
> **Source**: [`src/platform/fbo_scaler.cpp`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/fbo_scaler.cpp)
> **Cycle scale modes**: **F4**

The FBO scaler is the core rendering pipeline that decouples the game's internal resolution from the desktop window size. The game renders at its native **960×544** resolution into a framebuffer object (FBO), then the scaler applies optional post-processing effects and upscales the result to fill the desktop window.

---

## Pipeline Overview

```
┌───────────────────────────────────────────────────────────────┐
│                     Game Rendering (960×544)                  │
│  drawApplication() renders to FBO with color + depth textures│
└──────────────────────────┬────────────────────────────────────┘
                           ↓
┌──────────────────────────┴────────────────────────────────────┐
│                   Portal Effect Pass                          │
│  If g_portal_active: render swirling portal quad via          │
│  VERT_PORTAL + FRAG_PORTAL shaders with PVR texture          │
└──────────────────────────┬────────────────────────────────────┘
                           ↓
┌──────────────────────────┴────────────────────────────────────┐
│                PostFX Passes (if enabled)                     │
│                                                               │
│  Tier 2 (multi-pass, separate FBOs):                         │
│    ├─ SSAO → Gaussian blur → half-res AO texture             │
│    ├─ God Rays → half-res rays texture                       │
│    ├─ Bloom → extract → quarter-res blur → bloom texture     │
│    └─ Composite: scene + AO + rays + bloom + shadows         │
│                                                               │
│  Tier 1 (single pass, full-res):                             │
│    └─ CA → Sharpen → Color → Grain → Vignette → Outlines    │
└──────────────────────────┬────────────────────────────────────┘
                           ↓
┌──────────────────────────┴────────────────────────────────────┐
│              Upscale to Window (win_w × win_h)                │
│  Scale mode selected by FBOScale enum (F4 to cycle)          │
└───────────────────────────────────────────────────────────────┘
```

---

## FBOScale Enum

```cpp
enum class FBOScale : int {
    SHARP_BILINEAR = 0,
    NEAREST        = 1,
    CRT_SCANLINE   = 2,
    FSR            = 3,
};
```

| Value | Name | Description | Shader |
|-------|------|-------------|--------|
| `0` | `SHARP_BILINEAR` | Sub-pixel-aware bilinear filtering. Sharpens texels while maintaining smooth edges. Best general-purpose upscaler | `FRAG_SHARP_BILINEAR` |
| `1` | `NEAREST` | Hard pixel snapping with no interpolation. Crisp but blocky at non-integer scales | `FRAG_NEAREST` |
| `2` | `CRT_SCANLINE` | Simulates a CRT monitor: barrel distortion, horizontal scanlines, vignette, and pixel darkening at screen edges | `FRAG_CRT` |
| `3` | `FSR` | AMD FidelityFX Super Resolution 1.0 (EASU). Edge-adaptive 4×4 Lanczos upscaling with directional sharpening and integrated RCAS pass. Best quality at high scale factors | `FRAG_FSR` |

### Cycling

Press **F4** to cycle through modes: `SHARP_BILINEAR → NEAREST → CRT_SCANLINE → FSR → SHARP_BILINEAR → ...`

```cpp
g_fbo_mode = static_cast<FBOScale>((static_cast<int>(g_fbo_mode) + 1) % 4);
```

---

## Public API Functions

### fbo_init

```cpp
bool fbo_init(int game_w, int game_h);
```

Initialize the FBO pipeline. Creates all GPU resources:

| Resource | Resolution | Format | Purpose |
|----------|-----------|--------|---------|
| Main FBO color | `game_w × game_h` | RGBA8 | Game scene color |
| Main FBO depth | `game_w × game_h` | DEPTH24_STENCIL8 | Depth buffer as **texture** (not renderbuffer) for shader sampling |
| PostFX FBO A | `game_w × game_h` | RGBA8 | Full-res ping-pong buffer A |
| PostFX FBO B | `game_w × game_h` | RGBA8 | Full-res ping-pong buffer B |
| Half-res FBO A/B | `game_w/2 × game_h/2` | RGBA8 | SSAO render + Gaussian blur |
| Bloom FBO A/B | `game_w/4 × game_h/4` | RGBA8 | Bloom extraction + blur |
| Fullscreen quad VAO/VBO | — | — | Persistent vertex data for all blit passes |

Also compiles and caches all GLSL shader programs and their uniform locations.

**Returns**: `true` if the main FBO is complete and ready, `false` on failure.

**Typical call**:
```cpp
fbo_init(960, 544);
```

### fbo_destroy

```cpp
void fbo_destroy();
```

Clean up all GPU resources: delete FBOs, textures, shader programs, and the fullscreen quad VAO/VBO. Called on shutdown.

### fbo_begin_game

```cpp
void fbo_begin_game();
```

Bind the main game FBO as the render target. Must be called **before** `drawApplication()`:

```cpp
fbo_begin_game();        // Redirect rendering to FBO
drawApplication();       // Game renders at 960×544
fbo_end_game_and_blit(); // PostFX + upscale to window
```

Sets viewport to `game_w × game_h` and clears color + depth buffers.

### fbo_end_game_and_blit

```cpp
void fbo_end_game_and_blit(int win_w, int win_h,
                           FBOScale mode = FBOScale::SHARP_BILINEAR,
                           const PostFXState* postfx = nullptr);
```

The main pipeline function. Called **after** the game has finished rendering:

1. Unbinds the game FBO (rendering returns to the default framebuffer)
2. If `postfx != nullptr && postfx->enabled`:
   - Renders portal effect (if active)
   - Runs all Tier 2 PostFX passes (SSAO, god rays, bloom, composite)
   - Runs Tier 1 PostFX pass (color effects, outlines)
3. Upscales the result to `win_w × win_h` using the selected `mode`
4. Increments the animation timer (`g_postfx_time`)

| Parameter | Description |
|-----------|-------------|
| `win_w` | Desktop window width in pixels |
| `win_h` | Desktop window height in pixels |
| `mode` | Upscale filter to use (default: Sharp-Bilinear) |
| `postfx` | PostFX configuration; pass `nullptr` to skip all effects |

### fbo_get_texture

```cpp
unsigned int fbo_get_texture();
```

Returns the OpenGL texture ID of the main game FBO's color attachment. Useful for external consumers that need to sample the rendered scene (e.g., screenshot capture).

### fbo_is_active

```cpp
bool fbo_is_active();
```

Returns `true` if the FBO pipeline initialized successfully and is ready for use. Returns `false` if `fbo_init()` failed (e.g., incomplete framebuffer on the GPU).

### fbo_draw_portal_vanilla

```cpp
void fbo_draw_portal_vanilla(int viewport_w, int viewport_h);
```

Renders the portal effect directly into the currently bound framebuffer, **without** requiring the full FBO/PostFX pipeline. This is the fallback path used when PostFX is disabled but portals still need to render.

- Uses the same portal vertex/fragment shaders as the PostFX path
- Reads portal state from SRE-exported globals (`g_portal_active`, `g_portal_world_x/y/z`, etc.)
- Loads the portal texture (`portal_effect_2x.pvr`) on first use
- Uses additive blending (`GL_SRC_ALPHA, GL_ONE`) for the glow effect
- Manages its own animation timer independent of `g_postfx_time`

**Call after** `drawApplication()` and **before** GUI overlay rendering.

---

## Upscale Filter Details

### Sharp-Bilinear

Calculates sub-pixel position within each texel and applies a `fwidth()`-based sharpening curve:

```glsl
vec2 texel_f = fract(texel);
vec2 frange  = clamp(texel_f / fwidth(texel), 0.0, 1.0);
vec2 uv_sharp = (texel_i + frange - 0.5) / u_tex_size;
```

This produces bilinear-quality smoothness with nearest-neighbor crispness. Best for integer or near-integer scale factors.

### Nearest Neighbor

Simple floor-based texel snapping:

```glsl
vec2 uv = (floor(v_uv * u_tex_size) + 0.5) / u_tex_size;
```

No interpolation. Produces sharp pixel art appearance.

### CRT Scanline

Applies three effects in sequence:

1. **Barrel distortion** — `uv += center * r² * 0.04` for subtle curvature
2. **Scanlines** — `sin(uv.y * tex_height * π) * 0.5 + 0.5`, modulating brightness by 25%
3. **Edge vignette** — `vig.x * vig.y * 15.0` for CRT corner darkening

Out-of-bounds UVs (from barrel distortion) render as black.

### FSR 1.0 (Edge-Adaptive Spatial Upscaling)

Based on AMD FidelityFX Super Resolution 1.0 EASU. The implementation includes:

1. **4×4 neighborhood sampling** — 16 texels around the target pixel
2. **Edge detection** — Computes horizontal and vertical gradients using Sobel-like differences
3. **Directional Lanczos2 filtering** — Stretches the filter kernel along detected edges for sharper results
4. **Anti-ringing clamp** — Result is clamped to the min/max of the 4 nearest texels to prevent ringing artifacts
5. **Integrated RCAS pass** — 15% blend of a sharpening kernel for additional clarity

```glsl
// Directional sharpness selection
if (edge_h > edge_v * 1.5) {
    // Horizontal edge: sharpen vertically
    w = lanczos2(d.x) * lanczos2(d.y * sharpness);
} else if (edge_v > edge_h * 1.5) {
    // Vertical edge: sharpen horizontally
    w = lanczos2(d.x * sharpness) * lanczos2(d.y);
} else {
    // No strong edge: isotropic
    w = lanczos2(d.x) * lanczos2(d.y);
}
```

---

## Portal Effect System

The portal effect renders animated swirling portals that appear in the game world. The effect data is exported by SRE (the guest-side ARM library) each frame.

### SRE-Exported Globals

| Global | Type | Description |
|--------|------|-------------|
| `g_portal_active` | `float` | `1.0` when a portal should be drawn, `0.0` otherwise |
| `g_portal_world_x/y/z` | `float` | Portal world position (from game's Matrix4) |
| `g_portal_color_r/g/b` | `float` | Portal tint color from level data |
| `g_portal_intensity` | `float` | Glow strength (passed as `u_alpha` to shader) |
| `g_portal_speed` | `float` | Animation speed from PortalEffectComponent |
| `g_portal_vp_matrix[16]` | `float[16]` | Model-view matrix from the game's Draw call |

### Portal Shader

The portal vertex shader applies the game's model-view matrix with a matching perspective projection (FOV = 20°, near = 50, far = 20000) to position the quad in screen space.

The fragment shader creates the swirling effect:

1. Convert UVs to polar coordinates (distance + angle from center)
2. Apply time-based angle rotation that increases toward center (swirl)
3. Add radial pull distortion: `sin(dist * 8 - t * 2) * 0.03`
4. Sample portal texture with swirled UVs
5. Blend with a second layer (slower rotation) for depth
6. Apply elliptical archway mask: `smoothstep(0.50, 0.38, eDist)`
7. Add center glow and pulsing intensity

The portal texture (`portal_effect_2x.pvr`) is loaded from game assets on first use.

---

## Background Atmosphere Data

The PostFX composite shader uses atmosphere data exported from SRE to create context-sensitive rendering:

| Global | Default | Description |
|--------|---------|-------------|
| `g_bg_depth` | `11000.0` | Distance to background plane (affects shadow depth filtering) |
| `g_bg_scale` | `1.0` | Background sprite scale |
| `g_bg_ambient_r/g/b` | `0.6 / 0.5 / 0.4` | Sky ambient color used for shadow fill light |

The composite shader uses ambient color to subtly tint dark areas:

```glsl
float shadow_mask = smoothstep(0.4, 0.1, luma);  // 1.0 in dark areas
scene += ambient * shadow_mask * 0.08;            // subtle fill light
```

---

## Performance Considerations

### GPU Resource Summary

| Resource | Count | Notes |
|----------|-------|-------|
| Framebuffer objects | 7 | Main + 2 full-res + 2 half-res + 2 quarter-res |
| Textures | 8+ | Color + depth + PostFX A/B + half A/B + bloom A/B + portal |
| Shader programs | 10 | Sharp, nearest, CRT, FSR, PostFX, god rays, SSAO, blur, composite, bloom extract, portal |
| Cached uniform locations | 60+ | Eliminates ~81 `glGetUniformLocation` calls per frame |
| VAO/VBO | 1 | Persistent fullscreen quad (4 vertices) |

### Optimization Features

1. **Cached uniform locations** — All `glGetUniformLocation` calls happen once at shader compile time, not per-frame
2. **Persistent fullscreen quad VAO** — Single VBO upload at init; all passes reuse the same geometry
3. **Half/quarter resolution passes** — SSAO and bloom run at reduced resolution for major performance savings
4. **Early-out checks** — PostFX skipped entirely when `!enabled`, portal skipped when `g_portal_active <= 0.5`
5. **VAO unbind after draw** — Critical for compatibility with the game's GL 1.x client-side vertex arrays

### Render Target Sizes (at 960×544)

| Pass | Resolution | Pixels | % of Full |
|------|-----------|--------|-----------|
| Full-res | 960×544 | 522,240 | 100% |
| Half-res (SSAO) | 480×272 | 130,560 | 25% |
| Quarter-res (Bloom) | 240×136 | 32,640 | 6.25% |
