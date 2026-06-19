// ============================================================
//  SwordigoDesktop — FBO + GLSL Rendering Pipeline
//  Game renders to FBO, then through optional post-processing
//  passes (SSAO, God Rays, Color FX), then upscaled to window.
// ============================================================

#pragma once
#include <cstdint>

// Upscale filter modes
enum class FBOScale : int {
    SHARP_BILINEAR = 0,
    NEAREST        = 1,
    CRT_SCANLINE   = 2,
    FSR            = 3,   // AMD FidelityFX Super Resolution 1.0 (EASU)
};

// Post-processing effects state
struct PostFXState {
    bool enabled = false;
    
    // --- Tier 1: Color effects ---
    bool vignette = false;
    float vignette_intensity = 0.3f;
    
    bool film_grain = false;
    float grain_intensity = 0.06f;
    
    bool chromatic_aberration = false;
    float ca_offset = 0.002f;
    
    bool color_adjust = false;
    float saturation = 1.0f;
    float contrast = 1.0f;
    float brightness = 0.0f;
    float warmth = 0.0f;
    
    bool sharpen = false;
    float sharpen_strength = 0.4f;
    
    // --- Tier 2: Advanced effects ---
    bool god_rays = false;
    float god_rays_intensity = 0.4f;    // 0.0-1.0
    float god_rays_decay = 0.96f;       // per-sample falloff
    float sun_x = 0.5f;                 // sun position in UV (0-1)
    float sun_y = 0.85f;                // top of screen
    
    bool ssao = false;
    float ssao_radius = 0.02f;          // sample radius in UV
    float ssao_intensity = 1.2f;        // darkening strength
    
    bool volumetric_light = false;
    float volumetric_intensity = 0.3f;
    
    bool bloom = false;
    float bloom_threshold = 0.7f;       // brightness threshold for bloom extraction
    float bloom_intensity = 0.35f;      // additive bloom strength
    
    bool fake_shadows = false;
    float shadow_intensity = 0.3f;      // bottom-screen darkening
    float shadow_height = 0.25f;        // how high up the shadow gradient reaches
    
    // Preset name (for display)
    const char* preset_name = "Off";
};

enum class PostFXPreset : int {
    OFF = 0,
    SW_PLUS,        // Swordigo Plus — enhanced atmospheric with god rays
    ATMOSPHERIC,    // SSAO + volumetric + vignette
    ETHEREAL,       // God rays + warm glow
    CINEMATIC,
    RETRO,
    FANTASY,
    NOIR,
    CUSTOM,
    COUNT
};

void postfx_apply_preset(PostFXState& state, PostFXPreset preset);
const char* postfx_preset_name(PostFXPreset p);

bool fbo_init(int game_w, int game_h);
void fbo_destroy();
void fbo_begin_game();
void fbo_end_game_and_blit(int win_w, int win_h, FBOScale mode = FBOScale::SHARP_BILINEAR, const PostFXState* postfx = nullptr);
unsigned int fbo_get_texture();
bool fbo_is_active();
