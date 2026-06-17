// Unified pre-launch configuration window for Swordigo Desktop
// Borderless SDL2 + OpenGL window: Graphics API + Binary selection

#include "platform/launcher.h"
#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <algorithm>

// ============================================================================
// Embedded 8x8 bitmap font (ASCII 32–127)
// ============================================================================
static const uint8_t launcher_font[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    {0x6c,0x6c,0x6c,0x00,0x00,0x00,0x00,0x00},
    {0x36,0x36,0x7f,0x36,0x7f,0x36,0x36,0x00},
    {0x0c,0x3f,0x0c,0x0e,0x3c,0x0c,0x3e,0x0c},
    {0x00,0x66,0x66,0x30,0x18,0x0c,0x66,0x66},
    {0x3c,0x66,0x3c,0x38,0x67,0x66,0x3f,0x00},
    {0x06,0x0c,0x18,0x00,0x00,0x00,0x00,0x00},
    {0x0c,0x18,0x30,0x30,0x30,0x30,0x18,0x0c},
    {0x30,0x18,0x0c,0x0c,0x0c,0x0c,0x18,0x30},
    {0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00},
    {0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},
    {0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    {0x00,0x03,0x06,0x0c,0x18,0x30,0x60,0x00},
    {0x3e,0x63,0x67,0x6f,0x7b,0x63,0x3e,0x00},
    {0x0c,0x1c,0x0c,0x0c,0x0c,0x0c,0x3e,0x00},
    {0x3e,0x63,0x06,0x1c,0x30,0x60,0x7f,0x00},
    {0x7f,0x06,0x0c,0x1c,0x06,0x63,0x3e,0x00},
    {0x06,0x0f,0x1b,0x33,0x7f,0x03,0x03,0x00},
    {0x7f,0x60,0x7e,0x03,0x03,0x63,0x3e,0x00},
    {0x1c,0x30,0x60,0x7e,0x63,0x63,0x3e,0x00},
    {0x7f,0x03,0x06,0x0c,0x18,0x30,0x30,0x00},
    {0x3e,0x63,0x63,0x3e,0x63,0x63,0x3e,0x00},
    {0x3e,0x63,0x63,0x7f,0x03,0x06,0x3c,0x00},
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00},
    {0x06,0x0c,0x18,0x30,0x18,0x0c,0x06,0x00},
    {0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00},
    {0x60,0x30,0x18,0x0c,0x18,0x30,0x60,0x00},
    {0x3e,0x63,0x06,0x0c,0x18,0x00,0x18,0x00},
    {0x3e,0x63,0x6f,0x6b,0x6b,0x60,0x3e,0x00},
    {0x18,0x3c,0x66,0x66,0x7e,0x66,0x66,0x00},
    {0x7c,0x66,0x66,0x7c,0x66,0x66,0x7c,0x00},
    {0x3e,0x63,0x60,0x60,0x60,0x63,0x3e,0x00},
    {0x78,0x6c,0x66,0x66,0x66,0x6c,0x78,0x00},
    {0x7e,0x60,0x60,0x7c,0x60,0x60,0x7e,0x00},
    {0x7e,0x60,0x60,0x7c,0x60,0x60,0x60,0x00},
    {0x3e,0x63,0x60,0x6e,0x63,0x63,0x3e,0x00},
    {0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x00},
    {0x3e,0x0c,0x0c,0x0c,0x0c,0x0c,0x3e,0x00},
    {0x1f,0x06,0x06,0x06,0x06,0x66,0x3c,0x00},
    {0x66,0x6c,0x78,0x70,0x78,0x6c,0x66,0x00},
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7f,0x00},
    {0x63,0x77,0x7f,0x6b,0x63,0x63,0x63,0x00},
    {0x63,0x63,0x67,0x6f,0x7b,0x73,0x63,0x00},
    {0x3e,0x63,0x63,0x63,0x63,0x63,0x3e,0x00},
    {0x7c,0x66,0x66,0x7c,0x60,0x60,0x60,0x00},
    {0x3e,0x63,0x63,0x63,0x6b,0x66,0x3d,0x00},
    {0x7c,0x66,0x66,0x7c,0x78,0x6c,0x66,0x00},
    {0x3e,0x63,0x38,0x0e,0x07,0x63,0x3e,0x00},
    {0x7f,0x1c,0x1c,0x1c,0x1c,0x1c,0x1c,0x00},
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3c,0x00},
    {0x66,0x66,0x66,0x66,0x66,0x3c,0x18,0x00},
    {0x63,0x63,0x63,0x6b,0x7f,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1c,0x36,0x63,0x63,0x00},
    {0x66,0x66,0x66,0x3c,0x18,0x18,0x18,0x00},
    {0x7f,0x06,0x0c,0x18,0x30,0x60,0x7f,0x00},
    {0x3c,0x30,0x30,0x30,0x30,0x30,0x3c,0x00},
    {0x00,0x60,0x30,0x18,0x0c,0x06,0x03,0x00},
    {0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0x00},
    {0x08,0x1c,0x36,0x63,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
    {0x18,0x18,0x0c,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3c,0x06,0x3e,0x66,0x3b,0x00},
    {0x60,0x60,0x7c,0x66,0x66,0x66,0x7c,0x00},
    {0x00,0x00,0x3c,0x60,0x60,0x62,0x3c,0x00},
    {0x06,0x06,0x3e,0x66,0x66,0x66,0x3e,0x00},
    {0x00,0x00,0x3c,0x66,0x7e,0x60,0x3c,0x00},
    {0x0e,0x18,0x3e,0x18,0x18,0x18,0x18,0x00},
    {0x00,0x00,0x3e,0x66,0x66,0x3e,0x06,0x3c},
    {0x60,0x60,0x7c,0x66,0x66,0x66,0x66,0x00},
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3c,0x00},
    {0x06,0x00,0x0e,0x06,0x06,0x06,0x06,0x3c},
    {0x60,0x60,0x66,0x6c,0x78,0x6c,0x66,0x00},
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3c,0x00},
    {0x00,0x00,0x66,0x7f,0x6b,0x6b,0x63,0x00},
    {0x00,0x00,0x7c,0x66,0x66,0x66,0x66,0x00},
    {0x00,0x00,0x3c,0x66,0x66,0x66,0x3c,0x00},
    {0x00,0x00,0x7c,0x66,0x66,0x7c,0x60,0x60},
    {0x00,0x00,0x3e,0x66,0x66,0x3e,0x06,0x06},
    {0x00,0x00,0x7c,0x66,0x60,0x60,0x60,0x00},
    {0x00,0x00,0x3e,0x60,0x3c,0x06,0x7c,0x00},
    {0x18,0x18,0x7e,0x18,0x18,0x18,0x0d,0x06},
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3b,0x00},
    {0x00,0x00,0x66,0x66,0x66,0x3c,0x18,0x00},
    {0x00,0x00,0x63,0x6b,0x6b,0x7f,0x36,0x00},
    {0x00,0x00,0x66,0x3c,0x18,0x3c,0x66,0x00},
    {0x00,0x00,0x66,0x66,0x66,0x3e,0x06,0x3c},
    {0x00,0x00,0x7e,0x0c,0x18,0x30,0x7e,0x00},
    {0x0c,0x18,0x18,0x30,0x18,0x18,0x0c,0x00},
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    {0x30,0x18,0x18,0x0c,0x18,0x18,0x30,0x00},
    {0x3b,0x6e,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x1c,0x36,0x36,0x1c,0x00,0x00,0x00,0x00}
};

// ============================================================================
// Local drawing helpers
// ============================================================================
namespace lgfx {

static void rect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    glColor4ub(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x+w, y); glVertex2f(x+w, y+h); glVertex2f(x, y+h);
    glEnd();
}

static void border(float x, float y, float w, float h, float t, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    glColor4ub(r, g, b, a);
    glLineWidth(t);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x+w, y); glVertex2f(x+w, y+h); glVertex2f(x, y+h);
    glEnd();
}

static void text(const char* str, float x, float y, float scale, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    float cx = x;
    glColor4ub(r, g, b, a);
    for (const char* p = str; *p; p++) {
        if (*p == '\n') { y -= 12.0f * scale; cx = x; continue; }
        int idx = (uint8_t)*p - 32;
        if (idx < 0 || idx >= 96) { cx += 8.0f * scale; continue; }
        const uint8_t* glyph = launcher_font[idx];
        glBegin(GL_QUADS);
        for (int row = 0; row < 8; ++row) {
            uint8_t rd = glyph[row];
            for (int col = 0; col < 8; ++col) {
                if (rd & (1 << (7 - col))) {
                    float px = cx + col * scale;
                    float py = y + (7 - row) * scale;
                    glVertex2f(px, py); glVertex2f(px+scale, py);
                    glVertex2f(px+scale, py+scale); glVertex2f(px, py+scale);
                }
            }
        }
        glEnd();
        cx += 8.0f * scale;
    }
}

static float tw(const char* str, float scale) { return strlen(str) * 8.0f * scale; }

static bool hit(int mx, int my, float x, float y, float w, float h) {
    return mx >= (int)x && mx < (int)(x+w) && my >= (int)y && my < (int)(y+h);
}

} // namespace lgfx

// ============================================================================
// show_launcher()
// ============================================================================

LaunchConfig show_launcher(BinarySelector& selector) {
    using namespace lgfx;

    LaunchConfig cfg;
    cfg.graphics_api = GraphicsAPI::OPENGL;
    cfg.should_launch = true;

    const auto& bins = selector.get_binaries();
    bool has_bins = bins.size() > 1;
    int bin_sel = 0;
    // Pre-select default binary
    for (size_t i = 0; i < bins.size(); i++) {
        if (bins[i].is_default) bin_sel = (int)i;
    }

    // Window size — 50% bigger than original 600x420
    const int WIN_W = 900;
    int bin_section_h = has_bins ? (int)(bins.size() * 65 + 50) : 0;
    const int WIN_H = 630 + bin_section_h;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "[Launcher] SDL_Init failed: " << SDL_GetError() << std::endl;
        return cfg;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* win = SDL_CreateWindow("Swordigo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN);
    if (!win) {
        std::cerr << "[Launcher] Window create failed" << std::endl;
        return cfg;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { SDL_DestroyWindow(win); return cfg; }
    SDL_GL_SetSwapInterval(1);

    // State
    bool running = true;
    int api_sel = 0; // 0=OpenGL, 1=Vulkan
    int mouse_x = 0, mouse_y = 0;
    bool dragging = false;

    // Auto-launch timer (8s countdown if bins > 1)
    auto start_time = std::chrono::steady_clock::now();
    const int AUTO_LAUNCH_SEC = has_bins ? 10 : 0;

    // Layout
    const float PAD = 32.0f;
    const float W = (float)WIN_W;
    const float H = (float)WIN_H;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    cfg.should_launch = false;
                    running = false;
                    break;
                case SDL_MOUSEMOTION:
                    mouse_x = ev.motion.x;
                    mouse_y = WIN_H - ev.motion.y;
                    if (dragging) {
                        int wx, wy;
                        SDL_GetWindowPosition(win, &wx, &wy);
                        SDL_SetWindowPosition(win, wx + ev.motion.xrel, wy + ev.motion.yrel);
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        mouse_x = ev.button.x;
                        mouse_y = WIN_H - ev.button.y;
                        start_time = std::chrono::steady_clock::now(); // Reset countdown on click

                        // Close button (top-right)
                        if (hit(mouse_x, mouse_y, W - 44, H - 44, 36, 36)) {
                            cfg.should_launch = false;
                            running = false;
                            break;
                        }

                        // --- Graphics API radio buttons ---
                        float gfx_base = H - 200.0f;
                        float opt_w = W - PAD * 2 - 20.0f;
                        // OpenGL option
                        if (hit(mouse_x, mouse_y, PAD + 10, gfx_base - 5, opt_w, 62)) {
                            api_sel = 0; break;
                        }
                        // Vulkan option
                        if (hit(mouse_x, mouse_y, PAD + 10, gfx_base - 80, opt_w, 62)) {
                            api_sel = 1; break;
                        }

                        // --- Binary list clicks ---
                        if (has_bins) {
                            float bin_base = gfx_base - 80 - 62 - 50;
                            for (size_t i = 0; i < bins.size(); i++) {
                                float ey = bin_base - (float)i * 65.0f;
                                if (hit(mouse_x, mouse_y, PAD + 10, ey - 5, opt_w, 58)) {
                                    bin_sel = (int)i;
                                    break;
                                }
                            }
                        }

                        // --- Launch button ---
                        float btn_w = 260.0f, btn_h = 56.0f;
                        float btn_x = (W - btn_w) / 2.0f;
                        float btn_y = 40.0f;
                        if (hit(mouse_x, mouse_y, btn_x, btn_y, btn_w, btn_h)) {
                            cfg.graphics_api = (api_sel == 0) ? GraphicsAPI::OPENGL : GraphicsAPI::VULKAN;
                            if (has_bins && bin_sel >= 0 && bin_sel < (int)bins.size())
                                cfg.selected_binary = bins[bin_sel].filename;
                            cfg.should_launch = true;
                            running = false;
                            break;
                        }

                        // Drag (top bar)
                        if (ev.button.y < 50) dragging = true;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    dragging = false;
                    break;
                case SDL_KEYDOWN:
                    start_time = std::chrono::steady_clock::now(); // Reset countdown
                    if (ev.key.keysym.sym == SDLK_ESCAPE) {
                        cfg.should_launch = false; running = false;
                    }
                    if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) {
                        cfg.graphics_api = (api_sel == 0) ? GraphicsAPI::OPENGL : GraphicsAPI::VULKAN;
                        if (has_bins && bin_sel >= 0 && bin_sel < (int)bins.size())
                            cfg.selected_binary = bins[bin_sel].filename;
                        cfg.should_launch = true; running = false;
                    }
                    if (has_bins) {
                        if (ev.key.keysym.sym == SDLK_UP)
                            bin_sel = std::max(0, bin_sel - 1);
                        if (ev.key.keysym.sym == SDLK_DOWN)
                            bin_sel = std::min((int)bins.size() - 1, bin_sel + 1);
                    }
                    if (ev.key.keysym.sym == SDLK_TAB) {
                        api_sel = 1 - api_sel; // Toggle OpenGL/Vulkan
                    }
                    break;
            }
        }

        // Auto-launch
        int secs_left = 0;
        if (AUTO_LAUNCH_SEC > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            secs_left = AUTO_LAUNCH_SEC - (int)std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            if (secs_left <= 0) {
                cfg.graphics_api = (api_sel == 0) ? GraphicsAPI::OPENGL : GraphicsAPI::VULKAN;
                if (has_bins && bin_sel >= 0 && bin_sel < (int)bins.size())
                    cfg.selected_binary = bins[bin_sel].filename;
                cfg.should_launch = true;
                running = false;
            }
        }

        // ====================================================================
        // RENDER
        // ====================================================================
        glViewport(0, 0, WIN_W, WIN_H);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glOrtho(0, W, 0, H, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glClearColor(0.06f, 0.07f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // ── Outer border glow ──
        border(0, 0, W, H, 2.0f, 70, 130, 230, 180);
        border(1, 1, W-2, H-2, 1.0f, 35, 65, 115, 80);

        // ── Top bar (drag area) ──
        rect(0, H - 50, W, 50, 12, 14, 22, 255);
        rect(0, H - 52, W, 2, 70, 130, 230, 100);
        text("Swordigo", 18, H - 36, 1.8f, 70, 130, 230, 255);
        text("Launcher", 18 + tw("Swordigo ", 1.8f), H - 36, 1.8f, 120, 120, 140, 180);

        // Close [X]
        float cx_x = W - 44, cx_y = H - 44;
        bool cx_h = hit(mouse_x, mouse_y, cx_x, cx_y, 36, 36);
        rect(cx_x, cx_y, 36, 36, cx_h ? (uint8_t)180 : (uint8_t)60, 30, 30, cx_h ? (uint8_t)255 : (uint8_t)160);
        text("X", cx_x + 12, cx_y + 10, 1.8f, 255, 255, 255, 255);

        // ── Title ──
        float ty = H - 105;
        text("SWORDIGO", 50, ty, 4.0f, 70, 130, 230, 255);
        text("v2.0r - Release", 54, ty - 30, 1.5f, 120, 120, 140, 180);

        // Separator
        rect(PAD, ty - 48, W - PAD*2, 1, 70, 130, 230, 60);

        // ── GRAPHICS API section ──
        float gfx_label_y = H - 195;
        text("GRAPHICS API", PAD + 6, gfx_label_y, 1.6f, 160, 190, 255, 220);

        float opt_w = W - PAD * 2 - 20;
        float gfx_base = H - 220;

        // Option 1: OpenGL
        float oy1 = gfx_base;
        bool h1 = hit(mouse_x, mouse_y, PAD+10, oy1 - 5, opt_w, 62);
        rect(PAD+10, oy1-5, opt_w, 62,
             api_sel==0 ? (uint8_t)28 : (h1 ? (uint8_t)22 : (uint8_t)18),
             api_sel==0 ? (uint8_t)32 : (h1 ? (uint8_t)25 : (uint8_t)20),
             api_sel==0 ? (uint8_t)52 : (h1 ? (uint8_t)40 : (uint8_t)30), 240);
        if (api_sel==0) border(PAD+10, oy1-5, opt_w, 62, 1.5f, 70, 130, 230, 200);
        else if (h1) border(PAD+10, oy1-5, opt_w, 62, 1.0f, 50, 50, 70, 100);

        float rx = PAD + 28, ry = oy1 + 18;
        border(rx, ry, 16, 16, 1.5f, api_sel==0 ? (uint8_t)70 : (uint8_t)70, api_sel==0 ? (uint8_t)130 : (uint8_t)70, api_sel==0 ? (uint8_t)230 : (uint8_t)90, 200);
        if (api_sel==0) rect(rx+4, ry+4, 8, 8, 70, 130, 230, 255);
        text("OpenGL", rx+30, oy1+32, 1.8f, 210, 210, 225, 255);
        text("(Default)", rx+30+tw("OpenGL ",1.8f), oy1+32, 1.4f, 80, 200, 120, 200);
        text("Stable - Recommended for most systems", rx+30, oy1+6, 1.2f, 110, 110, 130, 170);

        // Option 2: Vulkan
        float oy2 = gfx_base - 80;
        bool h2 = hit(mouse_x, mouse_y, PAD+10, oy2 - 5, opt_w, 62);
        rect(PAD+10, oy2-5, opt_w, 62,
             api_sel==1 ? (uint8_t)28 : (h2 ? (uint8_t)22 : (uint8_t)18),
             api_sel==1 ? (uint8_t)28 : (h2 ? (uint8_t)22 : (uint8_t)18),
             api_sel==1 ? (uint8_t)48 : (h2 ? (uint8_t)38 : (uint8_t)28), 240);
        if (api_sel==1) border(PAD+10, oy2-5, opt_w, 62, 1.5f, 130, 90, 255, 200);
        else if (h2) border(PAD+10, oy2-5, opt_w, 62, 1.0f, 50, 50, 70, 100);

        float rx2 = PAD + 28, ry2 = oy2 + 18;
        border(rx2, ry2, 16, 16, 1.5f, api_sel==1 ? (uint8_t)130 : (uint8_t)70, api_sel==1 ? (uint8_t)90 : (uint8_t)70, api_sel==1 ? (uint8_t)255 : (uint8_t)90, 200);
        if (api_sel==1) rect(rx2+4, ry2+4, 8, 8, 130, 90, 255, 255);
        text("Vulkan", rx2+30, oy2+32, 1.8f, 210, 210, 225, 255);
        text("(WIP)", rx2+30+tw("Vulkan ",1.8f), oy2+32, 1.4f, 255, 180, 60, 200);
        text("Experimental - Better performance potential", rx2+30, oy2+6, 1.2f, 110, 110, 130, 170);

        // ── BINARY SELECTION section (only if >1 binary) ──
        if (has_bins) {
            float sep2_y = oy2 - 80;
            rect(PAD, sep2_y + 20, W - PAD*2, 1, 70, 130, 230, 60);

            text("GAME BINARY", PAD + 6, sep2_y + 5, 1.6f, 160, 190, 255, 220);

            if (AUTO_LAUNCH_SEC > 0 && secs_left > 0) {
                char cdown[64];
                snprintf(cdown, 64, "Auto-launching in %ds...", secs_left);
                text(cdown, W - PAD - tw(cdown, 1.1f), sep2_y + 8, 1.1f, 150, 150, 170, 180);
            }

            float bin_top = sep2_y - 15;
            for (size_t i = 0; i < bins.size(); i++) {
                const auto& b = bins[i];
                bool sel = ((int)i == bin_sel);
                float ey = bin_top - (float)i * 65.0f;
                bool ehov = hit(mouse_x, mouse_y, PAD+10, ey - 5, opt_w, 58);

                // Entry bg
                rect(PAD+10, ey-5, opt_w, 58,
                     sel ? (uint8_t)35 : (ehov ? (uint8_t)22 : (uint8_t)16),
                     sel ? (uint8_t)42 : (ehov ? (uint8_t)25 : (uint8_t)18),
                     sel ? (uint8_t)68 : (ehov ? (uint8_t)38 : (uint8_t)28), 240);
                if (sel) {
                    border(PAD+10, ey-5, opt_w, 58, 1.5f, 70, 130, 230, 180);
                    // Selection bar
                    rect(PAD+10, ey-5, 3, 58, 70, 180, 255, 255);
                }

                // Radio
                float brx = PAD + 28, bry = ey + 15;
                border(brx, bry, 14, 14, 1.5f, sel ? (uint8_t)70 : (uint8_t)60, sel ? (uint8_t)180 : (uint8_t)60, sel ? (uint8_t)255 : (uint8_t)80, 200);
                if (sel) rect(brx+3, bry+3, 8, 8, 70, 180, 255, 255);

                // Filename
                text(b.filename.c_str(), brx + 28, ey + 32, 1.5f, 220, 220, 240, 255);

                // Default badge
                if (b.is_default) {
                    float badge_x = brx + 28 + tw(b.filename.c_str(), 1.5f) + 12;
                    text("[DEFAULT]", badge_x, ey + 34, 1.0f, 70, 200, 130, 220);
                }

                // Detail line: version | status | size
                char detail[128];
                const char* st = (b.status == BinaryStatus::TESTED) ? "STABLE" :
                                 (b.status == BinaryStatus::TESTING) ? "TESTING" : "UNKNOWN";
                snprintf(detail, 128, "%s  |  %s  |  %.1f MB",
                         b.label.c_str(), st, (float)b.file_size / 1048576.0f);
                uint8_t dr = (b.status == BinaryStatus::TESTED) ? (uint8_t)100 : (b.status == BinaryStatus::TESTING) ? (uint8_t)255 : (uint8_t)160;
                uint8_t dg = (b.status == BinaryStatus::TESTED) ? (uint8_t)220 : (b.status == BinaryStatus::TESTING) ? (uint8_t)200 : (uint8_t)160;
                uint8_t db = (b.status == BinaryStatus::TESTED) ? (uint8_t)140 : (b.status == BinaryStatus::TESTING) ? (uint8_t)80 : (uint8_t)180;
                text(detail, brx + 28, ey + 10, 1.1f, dr, dg, db, 200);

                // SHA preview
                if (!b.sha256.empty()) {
                    std::string sha = "SHA256: " + b.sha256.substr(0, 20) + "...";
                    text(sha.c_str(), brx + 28, ey - 5, 0.95f, 80, 80, 100, 140);
                }
            }
        }

        // ── Separator above button ──
        rect(PAD, 108, W - PAD*2, 1, 40, 40, 55, 80);

        // ── LAUNCH button ──
        float btn_w = 260, btn_h = 56;
        float btn_x = (W - btn_w) / 2.0f;
        float btn_y = 40;
        bool btn_h_ = hit(mouse_x, mouse_y, btn_x, btn_y, btn_w, btn_h);

        rect(btn_x, btn_y, btn_w, btn_h,
             btn_h_ ? (uint8_t)90 : (uint8_t)70,
             btn_h_ ? (uint8_t)160 : (uint8_t)130,
             btn_h_ ? (uint8_t)255 : (uint8_t)230,
             btn_h_ ? (uint8_t)255 : (uint8_t)220);
        rect(btn_x, btn_y + btn_h - 2, btn_w, 2, 255, 255, 255, btn_h_ ? (uint8_t)50 : (uint8_t)25);
        text("LAUNCH", btn_x + (btn_w - tw("LAUNCH", 2.5f))/2, btn_y + 17, 2.5f, 255, 255, 255, 255);

        // ── Footer ──
        text("v2.0r - Release", PAD, 14, 1.1f, 50, 50, 65, 140);
        const char* hint = "ESC cancel  |  ENTER launch  |  TAB toggle API";
        text(hint, W - PAD - tw(hint, 1.0f), 14, 1.0f, 50, 50, 65, 140);

        SDL_GL_SwapWindow(win);
        SDL_Delay(16);
    }

    // Cleanup
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);

    // Set selection on the selector so main.cpp can read it
    if (has_bins) selector.selected_index = bin_sel;

    std::cout << "[Launcher] API: " << (cfg.graphics_api == GraphicsAPI::VULKAN ? "Vulkan" : "OpenGL")
              << " | Binary: " << cfg.selected_binary
              << (cfg.should_launch ? " | Launching" : " | Cancelled") << std::endl;

    return cfg;
}
