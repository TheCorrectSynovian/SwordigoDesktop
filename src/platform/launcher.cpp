// Unified pre-launch configuration window for Swordigo Desktop
// Borderless SDL3 + OpenGL window: Graphics API + Binary selection

#include "platform/launcher.h"
#include "platform/data_path.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#define GL_GLEXT_PROTOTYPES
#include "platform/gl_inc.h"
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

// Load a PNG file as an OpenGL texture. Returns texture ID (0 on failure).
static GLuint load_texture(const char* path) {
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) {
        std::cerr << "[Launcher] Cannot load texture: " << path << std::endl;
        return 0;
    }
    // Convert to RGBA
    SDL_Surface* rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surf);
    if (!rgba) return 0;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    SDL_DestroySurface(rgba);
    std::cout << "[Launcher] Loaded texture: " << path << " -> " << tex << std::endl;
    return tex;
}

// Draw a textured quad (full texture, tinted white)
static void texquad(GLuint tex, float x, float y, float w, float h, uint8_t a = 255) {
    if (!tex) return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4ub(255, 255, 255, a);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(x,   y);
    glTexCoord2f(1, 1); glVertex2f(x+w, y);
    glTexCoord2f(1, 0); glVertex2f(x+w, y+h);
    glTexCoord2f(0, 0); glVertex2f(x,   y+h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

// Draw a sub-region of a texture atlas as a stretched quad
static void texquad_region(GLuint tex, float x, float y, float w, float h,
                           float u0, float v0, float u1, float v1, uint8_t a = 255) {
    if (!tex) return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4ub(255, 255, 255, a);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v1); glVertex2f(x,   y);
    glTexCoord2f(u1, v1); glVertex2f(x+w, y);
    glTexCoord2f(u1, v0); glVertex2f(x+w, y+h);
    glTexCoord2f(u0, v0); glVertex2f(x,   y+h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
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

    // Window size — enlarged for Minecraft-style launcher feel
    const int WIN_W = 1100;
    int bin_section_h = has_bins ? (int)(bins.size() * 80 + 60) : 0;
    const int WIN_H = 700 + bin_section_h;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "[Launcher] SDL_Init failed: " << SDL_GetError() << std::endl;
        return cfg;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* win = SDL_CreateWindow("Swordigo",
        WIN_W, WIN_H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
    if (!win) {
        std::cerr << "[Launcher] Window create failed" << std::endl;
        return cfg;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { SDL_DestroyWindow(win); return cfg; }
    SDL_GL_SetSwapInterval(1);

    // Load launcher textures from src/assets/ (game assets repurposed for UI)
    // SDL3_image: no IMG_Init() needed — initialization is automatic
    std::string assets_base = get_data_path("src/assets");
    GLuint tex_bg       = load_texture((assets_base + "/launcher_bg.png").c_str());
    GLuint tex_ui_atlas = load_texture((assets_base + "/ui_atlas.png").c_str());
    GLuint tex_panel    = load_texture((assets_base + "/ui_panel.png").c_str());
    GLuint tex_button   = load_texture((assets_base + "/ui_button_wide.png").c_str());
    GLuint tex_bar      = load_texture((assets_base + "/ui_bar.png").c_str());
    GLuint tex_grove    = load_texture((assets_base + "/grove_bg.png").c_str());

    // State
    bool running = true;
    int api_sel = 0; // 0=OpenGL, 1=Vulkan
    int mouse_x = 0, mouse_y = 0;
    bool dragging = false;

    // Auto-launch disabled per user request
    auto start_time = std::chrono::steady_clock::now();
    const int AUTO_LAUNCH_SEC = 0;

    // Layout
    const float PAD = 40.0f;
    const float W = (float)WIN_W;
    const float H = (float)WIN_H;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT:
                    cfg.should_launch = false;
                    running = false;
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    mouse_x = ev.motion.x;
                    mouse_y = WIN_H - ev.motion.y;
                    if (dragging) {
                        int wx, wy;
                        SDL_GetWindowPosition(win, &wx, &wy);
                        SDL_SetWindowPosition(win, wx + ev.motion.xrel, wy + ev.motion.yrel);
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        mouse_x = ev.button.x;
                        mouse_y = WIN_H - ev.button.y;
                        start_time = std::chrono::steady_clock::now(); // Reset countdown on click

                        // Close button (top-right)
                        if (hit(mouse_x, mouse_y, W - 50, H - 50, 40, 40)) {
                            cfg.should_launch = false;
                            running = false;
                            break;
                        }

                        // --- Graphics API radio buttons ---
                        float gfx_base = H - 245.0f;
                        float opt_w = W - PAD * 2 - 20.0f;
                        // OpenGL option
                        if (hit(mouse_x, mouse_y, PAD + 10, gfx_base - 5, opt_w, 72)) {
                            api_sel = 0; break;
                        }
                        // Vulkan option
                        if (hit(mouse_x, mouse_y, PAD + 10, gfx_base - 90 - 5, opt_w, 72)) {
                            api_sel = 1; break;
                        }

                        // --- Binary list clicks ---
                        if (has_bins) {
                            float oy2 = gfx_base - 90;
                            float sep2_y = oy2 - 95;
                            float bin_top = sep2_y - 20;
                            for (size_t i = 0; i < bins.size(); i++) {
                                float ey = bin_top - (float)i * 80.0f;
                                if (hit(mouse_x, mouse_y, PAD + 10, ey - 5, opt_w, 72)) {
                                    bin_sel = (int)i;
                                    break;
                                }
                            }
                        }

                        // --- Launch button ---
                        float btn_w = 320.0f, btn_h = 64.0f;
                        float btn_x = (W - btn_w) / 2.0f;
                        float btn_y = 40.0f;
                        if (hit(mouse_x, mouse_y, btn_x, btn_y, btn_w, btn_h)) {
                            cfg.graphics_api = (api_sel == 0) ? GraphicsAPI::OPENGL : GraphicsAPI::VULKAN;
                            if (has_bins && bin_sel >= 0 && bin_sel < (int)bins.size()) {
                                cfg.selected_binary = bins[bin_sel].filename;
                                cfg.assets_dir = bins[bin_sel].assets_dir;
                                cfg.game_type = bins[bin_sel].game_type;
                            }
                            cfg.should_launch = true;
                            running = false;
                            break;
                        }

                        // Drag (top bar)
                        if (ev.button.y < 50) dragging = true;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    dragging = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    start_time = std::chrono::steady_clock::now(); // Reset countdown
                    if (ev.key.key == SDLK_ESCAPE) {
                        cfg.should_launch = false; running = false;
                    }
                    if (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER) {
                        cfg.graphics_api = (api_sel == 0) ? GraphicsAPI::OPENGL : GraphicsAPI::VULKAN;
                        if (has_bins && bin_sel >= 0 && bin_sel < (int)bins.size()) {
                            cfg.selected_binary = bins[bin_sel].filename;
                            cfg.assets_dir = bins[bin_sel].assets_dir;
                            cfg.game_type = bins[bin_sel].game_type;
                        }
                        cfg.should_launch = true; running = false;
                    }
                    if (has_bins) {
                        if (ev.key.key == SDLK_UP)
                            bin_sel = std::max(0, bin_sel - 1);
                        if (ev.key.key == SDLK_DOWN)
                            bin_sel = std::min((int)bins.size() - 1, bin_sel + 1);
                    }
                    if (ev.key.key == SDLK_TAB) {
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
                if (has_bins && bin_sel >= 0 && bin_sel < (int)bins.size()) {
                    cfg.selected_binary = bins[bin_sel].filename;
                    cfg.assets_dir = bins[bin_sel].assets_dir;
                    cfg.game_type = bins[bin_sel].game_type;
                }
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

        glClearColor(0.04f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // ── Starry sky background (from game's menu_back texture) ──
        texquad(tex_bg, 0, 0, W, H);

        // ── Outer border glow ──
        border(0, 0, W, H, 2.0f, 70, 130, 230, 180);
        border(1, 1, W-2, H-2, 1.0f, 35, 65, 115, 80);

        // ── Top bar: game's brown bar texture ──
        texquad(tex_bar, 0, H - 56, W, 56, 220);
        rect(0, H - 56, W, 56, 10, 12, 20, 160); // dark overlay
        rect(0, H - 58, W, 2, 70, 130, 230, 100);
        text("Swordigo", 22, H - 40, 2.5f, 70, 130, 230, 255);
        text("Desktop Launcher", 22 + tw("Swordigo ", 2.5f), H - 40, 2.5f, 100, 100, 120, 180);

        // Close [X]
        float cx_x = W - 50, cx_y = H - 50;
        bool cx_h = hit(mouse_x, mouse_y, cx_x, cx_y, 40, 40);
        rect(cx_x, cx_y, 40, 40, cx_h ? (uint8_t)180 : (uint8_t)60, 30, 30, cx_h ? (uint8_t)255 : (uint8_t)160);
        text("X", cx_x + 14, cx_y + 12, 2.0f, 255, 255, 255, 255);

        // ── Title with game panel backdrop ──
        float ty = H - 120;
        // Game UI panel behind title
        texquad(tex_panel, 30, ty - 50, W - 60, 100, 180);
        rect(30, ty - 50, W - 60, 100, 8, 10, 18, 120); // dark overlay for readability
        text("SWORDIGO", 60, ty, 5.0f, 90, 165, 255, 255);
        text("DESKTOP", 60 + tw("SWORDIGO ", 5.0f), ty, 5.0f, 60, 110, 200, 180);
        text("v2.0r - Release Build", 64, ty - 36, 1.8f, 140, 140, 160, 200);

        // Separator
        rect(PAD, ty - 56, W - PAD*2, 1, 70, 130, 230, 60);

        // ── GRAPHICS API section (panel backdrop) ──
        float gfx_label_y = H - 215;
        texquad(tex_panel, PAD - 5, gfx_label_y - 180, W - PAD*2 + 10, 200, 60);
        text("GRAPHICS API", PAD + 6, gfx_label_y, 2.0f, 160, 190, 255, 220);

        float opt_w = W - PAD * 2 - 20;
        float gfx_base = H - 245;

        // Option 1: OpenGL
        float oy1 = gfx_base;
        bool h1 = hit(mouse_x, mouse_y, PAD+10, oy1 - 5, opt_w, 72);
        rect(PAD+10, oy1-5, opt_w, 72,
             api_sel==0 ? (uint8_t)28 : (h1 ? (uint8_t)22 : (uint8_t)18),
             api_sel==0 ? (uint8_t)32 : (h1 ? (uint8_t)25 : (uint8_t)20),
             api_sel==0 ? (uint8_t)52 : (h1 ? (uint8_t)40 : (uint8_t)30), 240);
        if (api_sel==0) border(PAD+10, oy1-5, opt_w, 72, 1.5f, 70, 130, 230, 200);
        else if (h1) border(PAD+10, oy1-5, opt_w, 72, 1.0f, 50, 50, 70, 100);

        float rx = PAD + 30, ry = oy1 + 22;
        border(rx, ry, 18, 18, 1.5f, api_sel==0 ? (uint8_t)70 : (uint8_t)70, api_sel==0 ? (uint8_t)130 : (uint8_t)70, api_sel==0 ? (uint8_t)230 : (uint8_t)90, 200);
        if (api_sel==0) rect(rx+4, ry+4, 10, 10, 70, 130, 230, 255);
        text("OpenGL", rx+32, oy1+38, 2.2f, 210, 210, 225, 255);
        text("(Default)", rx+32+tw("OpenGL ",2.2f), oy1+40, 1.8f, 80, 200, 120, 200);
        text("Stable - Recommended for most systems", rx+32, oy1+8, 1.5f, 110, 110, 130, 170);

        // Option 2: Vulkan
        float oy2 = gfx_base - 90;
        bool h2 = hit(mouse_x, mouse_y, PAD+10, oy2 - 5, opt_w, 72);
        rect(PAD+10, oy2-5, opt_w, 72,
             api_sel==1 ? (uint8_t)28 : (h2 ? (uint8_t)22 : (uint8_t)18),
             api_sel==1 ? (uint8_t)28 : (h2 ? (uint8_t)22 : (uint8_t)18),
             api_sel==1 ? (uint8_t)48 : (h2 ? (uint8_t)38 : (uint8_t)28), 240);
        if (api_sel==1) border(PAD+10, oy2-5, opt_w, 72, 1.5f, 130, 90, 255, 200);
        else if (h2) border(PAD+10, oy2-5, opt_w, 72, 1.0f, 50, 50, 70, 100);

        float rx2 = PAD + 30, ry2 = oy2 + 22;
        border(rx2, ry2, 18, 18, 1.5f, api_sel==1 ? (uint8_t)130 : (uint8_t)70, api_sel==1 ? (uint8_t)90 : (uint8_t)70, api_sel==1 ? (uint8_t)255 : (uint8_t)90, 200);
        if (api_sel==1) rect(rx2+4, ry2+4, 10, 10, 130, 90, 255, 255);
        text("Vulkan", rx2+32, oy2+38, 2.2f, 210, 210, 225, 255);
        text("(WIP)", rx2+32+tw("Vulkan ",2.2f), oy2+40, 1.8f, 255, 180, 60, 200);
        text("Experimental - Better performance potential", rx2+32, oy2+8, 1.5f, 110, 110, 130, 170);

        // ── INSTALLED INSTANCES section ──
        if (has_bins) {
            float sep2_y = oy2 - 95;
            // Panel backdrop for instance list
            float bin_panel_h = (float)bins.size() * 80.0f + 60.0f;
            texquad(tex_panel, PAD - 5, sep2_y - bin_panel_h + 25, W - PAD*2 + 10, bin_panel_h + 20, 50);
            rect(PAD, sep2_y + 25, W - PAD*2, 1, 70, 130, 230, 60);

            text("INSTALLED INSTANCES", PAD + 6, sep2_y + 5, 2.4f, 160, 190, 255, 240);

            float bin_top = sep2_y - 20;
            for (size_t i = 0; i < bins.size(); i++) {
                const auto& b = bins[i];
                bool sel = ((int)i == bin_sel);
                float ey = bin_top - (float)i * 80.0f;
                bool ehov = hit(mouse_x, mouse_y, PAD+10, ey - 5, opt_w, 72);

                // Entry bg — orange tint for RL, blue for vanilla
                bool is_rl = (b.game_type == "RLSwordigo");
                rect(PAD+10, ey-5, opt_w, 72,
                     sel ? (is_rl ? (uint8_t)40 : (uint8_t)35) : (ehov ? (uint8_t)22 : (uint8_t)16),
                     sel ? (is_rl ? (uint8_t)32 : (uint8_t)42) : (ehov ? (uint8_t)25 : (uint8_t)18),
                     sel ? (is_rl ? (uint8_t)25 : (uint8_t)68) : (ehov ? (uint8_t)38 : (uint8_t)28), 240);
                if (sel) {
                    uint8_t bdr = is_rl ? (uint8_t)230 : (uint8_t)70;
                    uint8_t bdg = is_rl ? (uint8_t)140 : (uint8_t)130;
                    uint8_t bdb = is_rl ? (uint8_t)50 : (uint8_t)230;
                    border(PAD+10, ey-5, opt_w, 72, 2.0f, bdr, bdg, bdb, 200);
                    rect(PAD+10, ey-5, 5, 72, bdr, bdg, bdb, 255);
                }

                // > Arrow selector (game-style) instead of radio button
                float arrow_x = PAD + 20;
                if (sel) {
                    text(">", arrow_x, ey + 22, 3.0f,
                         is_rl ? (uint8_t)255 : (uint8_t)100,
                         is_rl ? (uint8_t)180 : (uint8_t)200,
                         is_rl ? (uint8_t)50 : (uint8_t)255, 255);
                } else if (ehov) {
                    text(">", arrow_x, ey + 22, 2.5f, 120, 120, 140, 120);
                }

                // Game type badge + Filename
                float name_x = PAD + 52;
                if (is_rl) {
                    rect(name_x, ey + 48, tw("[RL MOD]", 1.6f) + 10, 20, 230, 120, 30, 220);
                    text("[RL MOD]", name_x + 5, ey + 50, 1.6f, 255, 255, 255, 255);
                    name_x += tw("[RL MOD]", 1.6f) + 18;
                }
                text(b.filename.c_str(), name_x, ey + 46, 2.2f, 230, 230, 250, 255);

                // Default badge
                if (b.is_default) {
                    float badge_x = name_x + tw(b.filename.c_str(), 2.2f) + 14;
                    rect(badge_x, ey + 50, tw("DEFAULT", 1.3f) + 10, 16, 50, 160, 90, 200);
                    text("DEFAULT", badge_x + 5, ey + 51, 1.3f, 220, 255, 220, 255);
                }

                // Detail line: version | status | size
                char detail[192];
                const char* st = (b.status == BinaryStatus::TESTED) ? "STABLE" :
                                 (b.status == BinaryStatus::TESTING) ? "TESTING" : "UNKNOWN";
                snprintf(detail, 192, "%s  |  %s  |  %.1f MB  |  %s",
                         b.label.c_str(), st, (float)b.file_size / 1048576.0f, b.game_type.c_str());
                uint8_t dr = (b.status == BinaryStatus::TESTED) ? (uint8_t)110 : (b.status == BinaryStatus::TESTING) ? (uint8_t)255 : (uint8_t)160;
                uint8_t dg = (b.status == BinaryStatus::TESTED) ? (uint8_t)230 : (b.status == BinaryStatus::TESTING) ? (uint8_t)200 : (uint8_t)160;
                uint8_t db = (b.status == BinaryStatus::TESTED) ? (uint8_t)150 : (b.status == BinaryStatus::TESTING) ? (uint8_t)80 : (uint8_t)180;
                text(detail, PAD + 52, ey + 22, 1.5f, dr, dg, db, 210);

                // SHA256 (full hash, smaller)
                if (!b.sha256.empty()) {
                    std::string sha = "SHA: " + b.sha256;
                    text(sha.c_str(), PAD + 52, ey + 2, 1.2f, 80, 80, 100, 140);
                }
            }
        }

        // ── Separator above button ──
        rect(PAD, 120, W - PAD*2, 1, 40, 40, 55, 80);

        // ── LAUNCH button (game's brown button texture) ──
        float btn_w = 320, btn_h = 64;
        float btn_x = (W - btn_w) / 2.0f;
        float btn_y = 40;
        bool btn_h_ = hit(mouse_x, mouse_y, btn_x, btn_y, btn_w, btn_h);

        texquad(tex_button, btn_x, btn_y, btn_w, btn_h, btn_h_ ? (uint8_t)255 : (uint8_t)200);
        if (btn_h_) {
            rect(btn_x, btn_y, btn_w, btn_h, 70, 130, 230, 40); // blue hover tint
        }
        border(btn_x, btn_y, btn_w, btn_h, 1.5f, 70, 130, 230, btn_h_ ? (uint8_t)200 : (uint8_t)100);
        text("LAUNCH", btn_x + (btn_w - tw("LAUNCH", 3.0f))/2, btn_y + 20, 3.0f, 255, 255, 255, 255);

        // ── Footer bar (game's brown bar) ──
        texquad(tex_bar, 0, 0, W, 36, 140);
        rect(0, 0, W, 36, 8, 10, 16, 140); // dark overlay
        text("v2.0r - Release", PAD, 12, 1.4f, 60, 60, 80, 160);
        const char* hint = "ESC cancel  |  ENTER launch  |  TAB toggle API  |  UP/DOWN select";
        text(hint, W - PAD - tw(hint, 1.2f), 14, 1.2f, 60, 60, 80, 160);

        SDL_GL_SwapWindow(win);
        SDL_Delay(16);
    }

    // Cleanup textures
    GLuint textures[] = {tex_bg, tex_ui_atlas, tex_panel, tex_button, tex_bar, tex_grove};
    glDeleteTextures(6, textures);
    // SDL3_image: no IMG_Quit() needed

    // Cleanup SDL
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);

    // Set selection on the selector so main.cpp can read it
    if (has_bins) selector.selected_index = bin_sel;

    std::cout << "[Launcher] API: " << (cfg.graphics_api == GraphicsAPI::VULKAN ? "Vulkan" : "OpenGL")
              << " | Binary: " << cfg.selected_binary
              << (cfg.should_launch ? " | Launching" : " | Cancelled") << std::endl;

    return cfg;
}
