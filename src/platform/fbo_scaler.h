// ============================================================
//  SwordigoDesktop — FBO + GLSL Upscaling Renderer
//  Renders game to a native 960×544 FBO, then scales to window
//  using a sharp-bilinear shader (no blur, perfect pixel grid).
//
//  Usage (called from display.cpp):
//    fbo_init(960, 544);          — once at startup
//    fbo_begin_game();            — before ARM drawApp call
//    fbo_end_game_and_blit(win_w, win_h, mode);  — after drawApp
//    fbo_destroy();               — on shutdown
//
//  Upscale modes (selectable with F4 key):
//    0 = Sharp-bilinear (default) — integer scale + gentle smooth edge
//    1 = Nearest-neighbor         — raw pixels, maximum sharpness
//    2 = CRT scanline             — adds scanline emulation
// ============================================================

#pragma once
#include <cstdint>

// Upscale filter modes
enum class FBOScale : int {
    SHARP_BILINEAR = 0,  // Best for 2.5D games
    NEAREST        = 1,  // Raw pixels
    CRT_SCANLINE   = 2,  // Retro CRT look
};

bool fbo_init(int game_w, int game_h);
void fbo_destroy();

// Bind FBO so ARM GL calls render into it
void fbo_begin_game();

// Unbind FBO and blit to window using the upscaling shader
void fbo_end_game_and_blit(int win_w, int win_h, FBOScale mode = FBOScale::SHARP_BILINEAR);

// Get the FBO texture ID (for debug screenshots etc)
unsigned int fbo_get_texture();

// Returns true if FBO was successfully initialised
bool fbo_is_active();
