// Unified pre-launch configuration window for Swordigo Desktop
// PolyMC-inspired instance manager: 1100x650 borderless SDL3 + OpenGL
// Instance grid (left) + Detail panel (right) + toolbar + status bar

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
#include <filesystem>

namespace fs = std::filesystem;

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
// File dialog callback for "Add Instance"
// ============================================================================
struct FileDialogResult {
    std::string path;
    bool completed = false;
    bool cancelled = false;
};

static void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist, int filter) {
    auto* result = (FileDialogResult*)userdata;
    if (!filelist || !filelist[0]) {
        result->cancelled = true;
    } else {
        result->path = filelist[0];
    }
    result->completed = true;
}

// ============================================================================
// Launcher state
// ============================================================================
enum class LauncherState {
    NORMAL,
    NAMING_INSTANCE  // Text input for new instance name
};

// ============================================================================
// show_launcher() — PolyMC-inspired instance manager
// ============================================================================

LaunchConfig show_launcher(BinarySelector& selector) {
    using namespace lgfx;

    LaunchConfig cfg;
    cfg.graphics_api = GraphicsAPI::OPENGL;
    cfg.should_launch = true;

    const auto& bins = selector.get_binaries();
    int bin_sel = 0;
    // Pre-select default binary
    for (size_t i = 0; i < bins.size(); i++) {
        if (bins[i].is_default) bin_sel = (int)i;
    }

    // Window size — wider PolyMC-style layout
    const int WIN_W = 1100;
    const int WIN_H = 650;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "[Launcher] SDL_Init failed: " << SDL_GetError() << std::endl;
        return cfg;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* win = SDL_CreateWindow("Swordigo Desktop",
        WIN_W, WIN_H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
    if (!win) {
        std::cerr << "[Launcher] Window create failed" << std::endl;
        return cfg;
    }
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Set launcher window icon (taskbar/Alt-Tab)
    {
        std::string icon_file = get_data_path("src/assets/launcer_icon.png");
        SDL_Surface* icon_surf = IMG_Load(icon_file.c_str());
        if (icon_surf) {
            SDL_SetWindowIcon(win, icon_surf);
            SDL_DestroySurface(icon_surf);
        }
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { SDL_DestroyWindow(win); return cfg; }
    SDL_GL_SetSwapInterval(1);

    // Load launcher textures
    std::string assets_base = get_data_path("src/assets");
    GLuint tex_bg       = load_texture((assets_base + "/launcher_bg.png").c_str());
    GLuint tex_ui_atlas = load_texture((assets_base + "/ui_atlas.png").c_str());
    GLuint tex_panel    = load_texture((assets_base + "/ui_panel.png").c_str());
    GLuint tex_button   = load_texture((assets_base + "/ui_button_wide.png").c_str());
    GLuint tex_bar      = load_texture((assets_base + "/ui_bar.png").c_str());
    GLuint tex_grove    = load_texture((assets_base + "/grove_bg.png").c_str());

    // Load default instance icons
    std::string icons_base = assets_base + "/icons";
    GLuint tex_icon_sw  = load_texture((icons_base + "/swordigo_default.png").c_str());
    GLuint tex_icon_rl  = load_texture((icons_base + "/rl_swordigo_default.png").c_str());

    // Load per-instance custom icons (or assign default)
    std::vector<GLuint> instance_icons;
    for (const auto& b : bins) {
        if (!b.icon_path.empty() && fs::exists(b.icon_path)) {
            instance_icons.push_back(load_texture(b.icon_path.c_str()));
        } else if (b.game_type == "RLSwordigo") {
            instance_icons.push_back(tex_icon_rl);
        } else {
            instance_icons.push_back(tex_icon_sw);
        }
    }

    // State
    bool running = true;
    int api_sel = 0; // 0=OpenGL, 1=Vulkan
    int mouse_x = 0, mouse_y = 0;
    bool dragging = false;
    LauncherState state = LauncherState::NORMAL;
    std::string input_text;
    FileDialogResult dialog_result;

    // Auto-launch countdown
    auto start_time = std::chrono::steady_clock::now();
    const int AUTO_LAUNCH_SEC = 0;  // Disabled — require explicit LAUNCH click

    // Layout constants
    const float W = (float)WIN_W;
    const float H = (float)WIN_H;
    const float TOOLBAR_H = 50.0f;
    const float STATUSBAR_H = 28.0f;
    const float PANEL_X = 715.0f;    // Detail panel starts here
    const float PANEL_W = W - PANEL_X;
    const float GRID_X = 15.0f;
    const float GRID_W = PANEL_X - GRID_X - 10.0f;
    const float CONTENT_TOP = H - TOOLBAR_H;
    const float CONTENT_BOT = STATUSBAR_H;

    // Stored button positions (updated during render, used during click)
    float last_lbtn_x = 0, last_lbtn_y = 0, last_lbtn_w = 260, last_lbtn_h = 46;

    // Instance card layout
    const int GRID_COLS = 3;
    const float CARD_W = 200.0f;
    const float CARD_H = 140.0f;
    const float CARD_PAD = 12.0f;
    const float GRID_START_X = GRID_X + 10.0f;

    // Helper lambda to do a launch
    auto do_launch = [&]() {
        cfg.graphics_api = (api_sel == 0) ? GraphicsAPI::OPENGL : GraphicsAPI::VULKAN;
        if (!bins.empty() && bin_sel >= 0 && bin_sel < (int)bins.size()) {
            cfg.selected_binary = bins[bin_sel].filepath;
            cfg.assets_dir = bins[bin_sel].assets_dir;
            cfg.game_type = bins[bin_sel].game_type;
        }
        cfg.should_launch = true;
        running = false;
    };

    auto reset_timer = [&]() {
        start_time = std::chrono::steady_clock::now();
    };

    while (running) {
        // Check if file dialog completed (from Add Instance)
        if (dialog_result.completed) {
            dialog_result.completed = false;
            if (!dialog_result.cancelled && !dialog_result.path.empty()) {
                // Switch to naming state
                state = LauncherState::NAMING_INSTANCE;
                input_text.clear();
                SDL_StartTextInput(win);
                reset_timer();
            }
            dialog_result.cancelled = false;
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT:
                    cfg.should_launch = false;
                    running = false;
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    mouse_x = ev.motion.x;
                    mouse_y = WIN_H - ev.motion.y; // Convert to GL coords
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
                        reset_timer();

                        // Close button [X] (top-right corner)
                        if (hit(mouse_x, mouse_y, W - 46, H - TOOLBAR_H + 7, 36, 36)) {
                            cfg.should_launch = false;
                            running = false;
                            break;
                        }

                        // Drag the window by top toolbar
                        if (ev.button.y < (int)TOOLBAR_H) {
                            // Check Add Instance button first
                            float add_bx = W - 280.0f, add_by = H - TOOLBAR_H + 6, add_bw = 220.0f, add_bh = 38.0f;
                            if (hit(mouse_x, mouse_y, add_bx, add_by, add_bw, add_bh)) {
                                // Open file dialog
                                SDL_DialogFileFilter filters[] = {{"Shared Libraries", "so"}};
                                SDL_ShowOpenFileDialog(file_dialog_callback, &dialog_result, win, filters, 1, nullptr, false);
                                break;
                            }
                            dragging = true;
                            break;
                        }

                        if (state == LauncherState::NAMING_INSTANCE) {
                            // In naming mode, clicks don't select cards
                            break;
                        }

                        // === Instance card clicks ===
                        if (!bins.empty()) {
                            float grid_top = CONTENT_TOP - 10.0f;
                            for (size_t i = 0; i < bins.size(); i++) {
                                int col = (int)i % GRID_COLS;
                                int row = (int)i / GRID_COLS;
                                float cx = GRID_START_X + col * (CARD_W + CARD_PAD);
                                float cy = grid_top - (row + 1) * (CARD_H + CARD_PAD);
                                if (hit(mouse_x, mouse_y, cx, cy, CARD_W, CARD_H)) {
                                    bin_sel = (int)i;
                                    break;
                                }
                            }
                        }

                        // === Detail panel clicks ===
                        if (mouse_x >= (int)PANEL_X && !bins.empty()) {
                            float dp_x = PANEL_X + 20.0f;
                            float dp_top = CONTENT_TOP - 20.0f;

                            // LAUNCH button — use stored position from last render
                            if (hit(mouse_x, mouse_y, last_lbtn_x, last_lbtn_y, last_lbtn_w, last_lbtn_h)) {
                                do_launch();
                                break;
                            }

                            // Graphics API radio buttons
                            float api_y_base = last_lbtn_y - 30.0f;
                            float radio_x = dp_x + 10.0f;
                            // OpenGL
                            float ogl_y = api_y_base - 24.0f;
                            if (hit(mouse_x, mouse_y, radio_x, ogl_y, 200.0f, 20.0f)) {
                                api_sel = 0; break;
                            }
                            // Vulkan
                            float vk_y = ogl_y - 24.0f;
                            if (hit(mouse_x, mouse_y, radio_x, vk_y, 200.0f, 20.0f)) {
                                api_sel = 1; break;
                            }

                            // Open Folder button
                            float ofbtn_w = 200.0f, ofbtn_h = 34.0f;
                            float ofbtn_x = PANEL_X + (PANEL_W - ofbtn_w) / 2.0f;
                            float ofbtn_y = vk_y - 30.0f;
                            if (hit(mouse_x, mouse_y, ofbtn_x, ofbtn_y, ofbtn_w, ofbtn_h)) {
                                if (bin_sel >= 0 && bin_sel < (int)bins.size()) {
                                    std::string folder = fs::path(bins[bin_sel].filepath).parent_path().string();
                                    std::string cmd = "xdg-open \"" + folder + "\" &";
                                    system(cmd.c_str());
                                }
                                break;
                            }

                            // Remove Instance button
                            float rmbtn_y = ofbtn_y - 40.0f;
                            if (hit(mouse_x, mouse_y, ofbtn_x, rmbtn_y, ofbtn_w, ofbtn_h)) {
                                if (bin_sel >= 0 && bin_sel < (int)bins.size()) {
                                    std::cout << "[Launcher] Removing instance: "
                                              << bins[bin_sel].label << std::endl;
                                    // Remove the icon entry
                                    if (bin_sel < (int)instance_icons.size()) {
                                        instance_icons.erase(instance_icons.begin() + bin_sel);
                                    }
                                    // Remove from selector's in-memory list
                                    selector.remove_instance(bin_sel);
                                    // Adjust selection
                                    const auto& after_bins = selector.get_binaries();
                                    if (after_bins.empty()) {
                                        bin_sel = 0;
                                    } else if (bin_sel >= (int)after_bins.size()) {
                                        bin_sel = (int)after_bins.size() - 1;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    dragging = false;
                    break;

                case SDL_EVENT_TEXT_INPUT:
                    if (state == LauncherState::NAMING_INSTANCE) {
                        input_text += ev.text.text;
                        reset_timer();
                    }
                    break;

                case SDL_EVENT_KEY_DOWN:
                    reset_timer();

                    if (state == LauncherState::NAMING_INSTANCE) {
                        if (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER) {
                            if (!input_text.empty()) {
                                std::cout << "[Launcher] Adding custom instance: " << input_text
                                          << " from " << dialog_result.path << std::endl;
                                selector.add_custom_instance(dialog_result.path, input_text, "assets");
                                // Re-select the last item (newly added)
                                const auto& updated_bins = selector.get_binaries();
                                if (!updated_bins.empty()) {
                                    bin_sel = (int)updated_bins.size() - 1;
                                }
                            }
                            state = LauncherState::NORMAL;
                            SDL_StopTextInput(win);
                            dialog_result.path.clear();
                        } else if (ev.key.key == SDLK_ESCAPE) {
                            state = LauncherState::NORMAL;
                            SDL_StopTextInput(win);
                            input_text.clear();
                            dialog_result.path.clear();
                        } else if (ev.key.key == SDLK_BACKSPACE && !input_text.empty()) {
                            input_text.pop_back();
                        }
                        break;
                    }

                    // Normal mode keys
                    if (ev.key.key == SDLK_ESCAPE) {
                        cfg.should_launch = false; running = false;
                    }
                    if (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER) {
                        do_launch();
                    }
                    if (!bins.empty()) {
                        if (ev.key.key == SDLK_RIGHT)
                            bin_sel = std::min((int)bins.size() - 1, bin_sel + 1);
                        if (ev.key.key == SDLK_LEFT)
                            bin_sel = std::max(0, bin_sel - 1);
                        if (ev.key.key == SDLK_DOWN)
                            bin_sel = std::min((int)bins.size() - 1, bin_sel + GRID_COLS);
                        if (ev.key.key == SDLK_UP)
                            bin_sel = std::max(0, bin_sel - GRID_COLS);
                    }
                    if (ev.key.key == SDLK_TAB) {
                        api_sel = 1 - api_sel;
                    }
                    break;
            }
        }

        // Re-read bins reference in case add_custom_instance modified the vector
        const auto& cur_bins = selector.get_binaries();
        if (bin_sel >= (int)cur_bins.size() && !cur_bins.empty())
            bin_sel = (int)cur_bins.size() - 1;

        // Auto-launch
        int secs_left = 0;
        if (AUTO_LAUNCH_SEC > 0 && state == LauncherState::NORMAL) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            secs_left = AUTO_LAUNCH_SEC - (int)std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            if (secs_left <= 0) {
                do_launch();
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

        // Background
        glClearColor(0.102f, 0.102f, 0.180f, 1.0f); // #1a1a2e
        glClear(GL_COLOR_BUFFER_BIT);

        // Starry sky background texture
        texquad(tex_bg, 0, 0, W, H, 140);

        // Outer border glow
        border(0, 0, W, H, 2.0f, 70, 130, 230, 180);
        border(1, 1, W-2, H-2, 1.0f, 35, 65, 115, 80);

        // ==== TOOLBAR (top 50px) ====
        texquad(tex_bar, 0, H - TOOLBAR_H, W, TOOLBAR_H, 220);
        rect(0, H - TOOLBAR_H, W, TOOLBAR_H, 16, 16, 30, 180);
        rect(0, H - TOOLBAR_H - 2, W, 2, 70, 130, 230, 100);

        // Title in toolbar
        text("SWORDIGO", 16, H - TOOLBAR_H + 14, 3.0f, 90, 165, 255, 255);
        text("DESKTOP", 16 + tw("SWORDIGO ", 3.0f), H - TOOLBAR_H + 14, 3.0f, 60, 110, 200, 160);

        // + Add Instance button
        float add_bx = W - 280.0f, add_by = H - TOOLBAR_H + 6, add_bw = 220.0f, add_bh = 38.0f;
        bool add_hov = hit(mouse_x, mouse_y, add_bx, add_by, add_bw, add_bh);
        rect(add_bx, add_by, add_bw, add_bh,
             add_hov ? (uint8_t)40 : (uint8_t)30,
             add_hov ? (uint8_t)45 : (uint8_t)35,
             add_hov ? (uint8_t)70 : (uint8_t)55, 240);
        border(add_bx, add_by, add_bw, add_bh, 1.0f, 70, 130, 230, 160);
        text("+ Add Instance", add_bx + 14, add_by + 10, 1.6f, 180, 200, 255, 255);

        // Close [X]
        float cx_x = W - 46, cx_y = H - TOOLBAR_H + 7;
        bool cx_h = hit(mouse_x, mouse_y, cx_x, cx_y, 36, 36);
        rect(cx_x, cx_y, 36, 36, cx_h ? (uint8_t)180 : (uint8_t)60, 30, 30, cx_h ? (uint8_t)255 : (uint8_t)160);
        text("X", cx_x + 12, cx_y + 10, 2.0f, 255, 255, 255, 255);

        // ==== DETAIL PANEL BACKGROUND (right side) ====
        rect(PANEL_X, CONTENT_BOT, PANEL_W, CONTENT_TOP - CONTENT_BOT, 37, 37, 62, 220);
        // Vertical separator line
        rect(PANEL_X - 1, CONTENT_BOT, 2, CONTENT_TOP - CONTENT_BOT, 70, 130, 230, 60);

        // ==== INSTANCE GRID (left side) ====
        float grid_top = CONTENT_TOP - 10.0f;

        // Naming instance bar (shown when adding)
        if (state == LauncherState::NAMING_INSTANCE) {
            float nb_y = grid_top - 36.0f;
            rect(GRID_X, nb_y, GRID_W, 36, 30, 30, 55, 240);
            border(GRID_X, nb_y, GRID_W, 36, 1.5f, 70, 200, 130, 200);
            text("Name: ", GRID_X + 8, nb_y + 10, 1.6f, 160, 200, 160, 255);
            float name_x = GRID_X + 8 + tw("Name: ", 1.6f);
            std::string display_text = input_text + "_";
            text(display_text.c_str(), name_x, nb_y + 10, 1.6f, 224, 224, 238, 255);
            text("(Enter to confirm, ESC to cancel)", GRID_X + 8, nb_y - 14, 1.0f, 136, 136, 170, 160);
            grid_top -= 56.0f;
        }

        if (!cur_bins.empty()) {
            for (size_t i = 0; i < cur_bins.size(); i++) {
                const auto& b = cur_bins[i];
                int col = (int)i % GRID_COLS;
                int row = (int)i / GRID_COLS;
                float cx = GRID_START_X + col * (CARD_W + CARD_PAD);
                float cy = grid_top - (row + 1) * (CARD_H + CARD_PAD);
                bool sel = ((int)i == bin_sel);
                bool hov = hit(mouse_x, mouse_y, cx, cy, CARD_W, CARD_H);
                bool is_rl = (b.game_type == "RLSwordigo");
                bool is_3d = (b.version_dir == "sw3d");

                // Card background
                uint8_t bg_r = sel ? (uint8_t)35 : (hov ? (uint8_t)30 : (uint8_t)25);
                uint8_t bg_g = sel ? (uint8_t)38 : (hov ? (uint8_t)32 : (uint8_t)25);
                uint8_t bg_b = sel ? (uint8_t)65 : (hov ? (uint8_t)50 : (uint8_t)40);
                rect(cx, cy, CARD_W, CARD_H, bg_r, bg_g, bg_b, 230);

                // Selected highlight border
                if (sel) {
                    border(cx, cy, CARD_W, CARD_H, 2.0f, 74, 144, 217, 230);
                } else if (hov) {
                    border(cx, cy, CARD_W, CARD_H, 1.0f, 70, 130, 230, 80);
                }

                // Instance icon (50x50, centered horizontally at top of card)
                float icon_x = cx + (CARD_W - 50) / 2.0f;
                float icon_y = cy + CARD_H - 10 - 50;
                GLuint icon_tex = (i < instance_icons.size()) ? instance_icons[i] : 0;
                if (icon_tex) {
                    // Render the actual icon texture
                    texquad(icon_tex, icon_x, icon_y, 50, 50);
                } else if (is_rl) {
                    rect(icon_x, icon_y, 50, 50, 239, 155, 91, 200);
                    text("RL", icon_x + 12, icon_y + 16, 2.0f, 255, 255, 255, 220);
                } else if (is_3d) {
                    rect(icon_x, icon_y, 50, 50, 176, 91, 239, 200);
                    text("3D", icon_x + 12, icon_y + 16, 2.0f, 255, 255, 255, 220);
                } else {
                    rect(icon_x, icon_y, 50, 50, 74, 144, 217, 200);
                    text("SW", icon_x + 12, icon_y + 16, 2.0f, 255, 255, 255, 220);
                }

                // Version text (below icon)
                std::string ver_text = "v" + b.version;
                float ver_x = cx + (CARD_W - tw(ver_text.c_str(), 1.6f)) / 2.0f;
                text(ver_text.c_str(), ver_x, icon_y - 16, 1.6f, 224, 224, 238, 255);

                // Arch badge
                const char* arch_str = BinarySelector::arch_string(b.arch);
                char badge_buf[16];
                snprintf(badge_buf, 16, "[%s]", arch_str);
                float badge_w = tw(badge_buf, 1.3f) + 8;
                float badge_x = cx + (CARD_W - badge_w) / 2.0f;
                float badge_y = icon_y - 32;
                if (b.arch == BinaryArch::ARM64) {
                    rect(badge_x, badge_y, badge_w, 16, 91, 239, 122, 180);
                } else {
                    rect(badge_x, badge_y, badge_w, 16, 91, 141, 239, 180);
                }
                text(badge_buf, badge_x + 4, badge_y + 3, 1.3f, 255, 255, 255, 255);

                // Status dot (bottom-left of card)
                float dot_x = cx + 8, dot_y = cy + 8;
                if (b.status == BinaryStatus::TESTED) {
                    rect(dot_x, dot_y, 8, 8, 50, 200, 50, 255);
                } else if (b.status == BinaryStatus::TESTING) {
                    rect(dot_x, dot_y, 8, 8, 200, 200, 50, 255);
                } else {
                    rect(dot_x, dot_y, 8, 8, 200, 50, 50, 255);
                }

                // Default star indicator
                if (b.is_default) {
                    text("*", cx + CARD_W - 16, cy + 6, 1.5f, 255, 220, 60, 200);
                }
            }
        } else {
            // No instances
            text("No game binaries found.", GRID_X + 40, H / 2.0f, 2.0f, 136, 136, 170, 200);
            text("Use [+ Add Instance] to add one.", GRID_X + 20, H / 2.0f - 30, 1.5f, 100, 100, 140, 160);
        }

        // ==== DETAIL PANEL (right side content) ====
        if (!cur_bins.empty() && bin_sel >= 0 && bin_sel < (int)cur_bins.size()) {
            const auto& sb = cur_bins[bin_sel];
            float dp_x = PANEL_X + 20.0f;
            float dp_top = CONTENT_TOP - 20.0f;
            float dp_cur = dp_top;
            bool is_rl = (sb.game_type == "RLSwordigo");
            bool is_3d = (sb.version_dir == "sw3d");

            // Large icon (70x70) — uses same instance icon textures
            float li_x = dp_x, li_y = dp_cur - 70;
            GLuint detail_icon = (bin_sel >= 0 && bin_sel < (int)instance_icons.size()) ? instance_icons[bin_sel] : 0;
            if (detail_icon) {
                texquad(detail_icon, li_x, li_y, 70, 70);
            } else if (is_rl) {
                rect(li_x, li_y, 70, 70, 239, 155, 91, 200);
                text("RL", li_x + 18, li_y + 22, 3.0f, 255, 255, 255, 220);
            } else if (is_3d) {
                rect(li_x, li_y, 70, 70, 176, 91, 239, 200);
                text("3D", li_x + 18, li_y + 22, 3.0f, 255, 255, 255, 220);
            } else {
                rect(li_x, li_y, 70, 70, 74, 144, 217, 200);
                text("SW", li_x + 18, li_y + 22, 3.0f, 255, 255, 255, 220);
            }

            // Name + version (to the right of icon)
            std::string title_str = (is_rl ? "RLSwordigo" : "Swordigo");
            title_str += " v" + sb.version;
            text(title_str.c_str(), li_x + 80, dp_cur - 24, 2.0f, 224, 224, 238, 255);

            // Arch badge next to title
            const char* arch_s = BinarySelector::arch_string(sb.arch);
            char abadge[16];
            snprintf(abadge, 16, "[%s]", arch_s);
            float abx = li_x + 80;
            float aby = dp_cur - 48;
            float abw = tw(abadge, 1.5f) + 8;
            if (sb.arch == BinaryArch::ARM64) {
                rect(abx, aby, abw, 18, 91, 239, 122, 180);
            } else {
                rect(abx, aby, abw, 18, 91, 141, 239, 180);
            }
            text(abadge, abx + 4, aby + 3, 1.5f, 255, 255, 255, 255);

            // Status text next to arch badge
            const char* status_text = (sb.status == BinaryStatus::TESTED) ? "Stable" :
                                      (sb.status == BinaryStatus::TESTING) ? "Testing" : "Unknown";
            text(status_text, abx + abw + 10, aby + 3, 1.5f,
                 sb.status == BinaryStatus::TESTED ? (uint8_t)50 : (sb.status == BinaryStatus::TESTING ? (uint8_t)200 : (uint8_t)200),
                 sb.status == BinaryStatus::TESTED ? (uint8_t)200 : (sb.status == BinaryStatus::TESTING ? (uint8_t)200 : (uint8_t)50),
                 sb.status == BinaryStatus::TESTED ? (uint8_t)50 : (sb.status == BinaryStatus::TESTING ? (uint8_t)50 : (uint8_t)50),
                 220);

            dp_cur = li_y - 12;
            // Separator
            rect(dp_x, dp_cur, PANEL_W - 40, 1, 70, 130, 230, 60);
            dp_cur -= 18;

            // Detail rows
            float row_h = 18.0f;

            // Version
            text("Version:", dp_x, dp_cur, 1.3f, 136, 136, 170, 200);
            std::string ver_detail = "v" + sb.version;
            text(ver_detail.c_str(), dp_x + tw("Version: ", 1.3f), dp_cur, 1.3f, 200, 200, 220, 220);
            dp_cur -= row_h;

            // Architecture
            text("Arch:", dp_x, dp_cur, 1.3f, 136, 136, 170, 200);
            text(BinarySelector::arch_string(sb.arch), dp_x + tw("Arch:  ", 1.3f), dp_cur, 1.3f, 200, 200, 220, 220);
            dp_cur -= row_h;

            // Game Type
            text("Type:", dp_x, dp_cur, 1.3f, 136, 136, 170, 200);
            text(sb.game_type.c_str(), dp_x + tw("Type:  ", 1.3f), dp_cur, 1.3f, 200, 200, 220, 220);
            dp_cur -= row_h;

            // Path
            text("Path:", dp_x, dp_cur, 1.3f, 136, 136, 170, 200);
            std::string path_display = sb.filepath;
            if (path_display.length() > 32) {
                path_display = "..." + path_display.substr(path_display.length() - 29);
            }
            text(path_display.c_str(), dp_x + tw("Path:  ", 1.3f), dp_cur, 1.3f, 200, 200, 220, 220);
            dp_cur -= row_h;

            // Size
            char size_buf[32];
            snprintf(size_buf, 32, "%.1f MB", (float)sb.file_size / 1048576.0f);
            text("Size:", dp_x, dp_cur, 1.3f, 136, 136, 170, 200);
            text(size_buf, dp_x + tw("Size:  ", 1.3f), dp_cur, 1.3f, 200, 200, 220, 220);
            dp_cur -= row_h;

            // SHA
            text("SHA:", dp_x, dp_cur, 1.3f, 136, 136, 170, 200);
            std::string sha_short = sb.sha256.substr(0, 16) + "...";
            text(sha_short.c_str(), dp_x + tw("SHA:   ", 1.3f), dp_cur, 1.3f, 200, 200, 220, 220);
            dp_cur -= row_h;

            // Assets Directory
            text("Assets:", dp_x, dp_cur, 1.3f, 136, 136, 170, 200);
            text(sb.assets_dir.c_str(), dp_x + tw("Assets:", 1.3f) + 8, dp_cur, 1.3f, 200, 200, 220, 220);
            dp_cur -= row_h;

            // Status with dot
            text("Status:", dp_x, dp_cur, 1.3f, 136, 136, 170, 200);
            float sdot_x = dp_x + tw("Status:", 1.3f) + 8;
            if (sb.status == BinaryStatus::TESTED) {
                rect(sdot_x, dp_cur + 2, 10, 10, 50, 200, 50, 255);
            } else if (sb.status == BinaryStatus::TESTING) {
                rect(sdot_x, dp_cur + 2, 10, 10, 200, 200, 50, 255);
            } else {
                rect(sdot_x, dp_cur + 2, 10, 10, 200, 50, 50, 255);
            }
            text(status_text, sdot_x + 16, dp_cur, 1.3f, 200, 200, 220, 220);
            dp_cur -= row_h;

            // Dependencies
            text("Deps:", dp_x, dp_cur, 1.3f, 136, 136, 170, 200);
            if (sb.dependencies.empty()) {
                text("(none)", dp_x + tw("Deps:  ", 1.3f), dp_cur, 1.3f, 200, 200, 220, 220);
            } else {
                std::string deps_str;
                for (size_t d = 0; d < sb.dependencies.size(); d++) {
                    if (d > 0) deps_str += ", ";
                    deps_str += sb.dependencies[d];
                }
                if (deps_str.length() > 30) deps_str = deps_str.substr(0, 27) + "...";
                text(deps_str.c_str(), dp_x + tw("Deps:  ", 1.3f), dp_cur, 1.3f, 200, 200, 220, 220);
            }
            dp_cur -= 26;

            // ==== LAUNCH BUTTON ====
            float lbtn_w = 260.0f, lbtn_h = 46.0f;
            float lbtn_x = PANEL_X + (PANEL_W - lbtn_w) / 2.0f;
            float lbtn_y = dp_cur - lbtn_h;
            // Store for click handler
            last_lbtn_x = lbtn_x; last_lbtn_y = lbtn_y;
            last_lbtn_w = lbtn_w; last_lbtn_h = lbtn_h;
            bool lbtn_hov = hit(mouse_x, mouse_y, lbtn_x, lbtn_y, lbtn_w, lbtn_h);
            texquad(tex_button, lbtn_x, lbtn_y, lbtn_w, lbtn_h, lbtn_hov ? (uint8_t)255 : (uint8_t)200);
            if (lbtn_hov) rect(lbtn_x, lbtn_y, lbtn_w, lbtn_h, 70, 130, 230, 60);
            border(lbtn_x, lbtn_y, lbtn_w, lbtn_h, 2.0f, 70, 130, 230, lbtn_hov ? (uint8_t)240 : (uint8_t)140);
            text("LAUNCH", lbtn_x + (lbtn_w - tw("LAUNCH", 2.4f))/2, lbtn_y + 14, 2.4f, 255, 255, 255, 255);
            dp_cur = lbtn_y - 20;

            // ==== GRAPHICS API SELECTOR ====
            text("GRAPHICS API", dp_x, dp_cur, 1.5f, 160, 190, 255, 220);
            dp_cur -= 6;
            float radio_x = dp_x + 10.0f;

            // OpenGL
            float ogl_y = dp_cur - 20.0f;
            bool ogl_hov = hit(mouse_x, mouse_y, radio_x, ogl_y, 200.0f, 20.0f);
            border(radio_x, ogl_y + 4, 12, 12, 1.5f,
                   api_sel==0 ? (uint8_t)70 : (uint8_t)60,
                   api_sel==0 ? (uint8_t)130 : (uint8_t)60,
                   api_sel==0 ? (uint8_t)230 : (uint8_t)80, 200);
            if (api_sel == 0) rect(radio_x + 3, ogl_y + 7, 6, 6, 70, 130, 230, 255);
            text("OpenGL", radio_x + 20, ogl_y + 4, 1.5f, 210, 210, 225, 255);
            text("(Default)", radio_x + 20 + tw("OpenGL ", 1.5f), ogl_y + 5, 1.2f, 80, 200, 120, 180);
            dp_cur = ogl_y - 4;

            // Vulkan
            float vk_y = dp_cur - 20.0f;
            bool vk_hov = hit(mouse_x, mouse_y, radio_x, vk_y, 200.0f, 20.0f);
            border(radio_x, vk_y + 4, 12, 12, 1.5f,
                   api_sel==1 ? (uint8_t)130 : (uint8_t)60,
                   api_sel==1 ? (uint8_t)90 : (uint8_t)60,
                   api_sel==1 ? (uint8_t)255 : (uint8_t)80, 200);
            if (api_sel == 1) rect(radio_x + 3, vk_y + 7, 6, 6, 130, 90, 255, 255);
            text("Vulkan", radio_x + 20, vk_y + 4, 1.5f, 210, 210, 225, 255);
            text("(WIP)", radio_x + 20 + tw("Vulkan ", 1.5f), vk_y + 5, 1.2f, 255, 180, 60, 180);
            dp_cur = vk_y - 20;

            // ==== OPEN FOLDER BUTTON ====
            float ofbtn_w = 200.0f, ofbtn_h = 34.0f;
            float ofbtn_x = PANEL_X + (PANEL_W - ofbtn_w) / 2.0f;
            float ofbtn_y = dp_cur - ofbtn_h;
            bool of_hov = hit(mouse_x, mouse_y, ofbtn_x, ofbtn_y, ofbtn_w, ofbtn_h);
            rect(ofbtn_x, ofbtn_y, ofbtn_w, ofbtn_h,
                 of_hov ? (uint8_t)40 : (uint8_t)30,
                 of_hov ? (uint8_t)45 : (uint8_t)35,
                 of_hov ? (uint8_t)70 : (uint8_t)55, 220);
            border(ofbtn_x, ofbtn_y, ofbtn_w, ofbtn_h, 1.0f, 70, 130, 230, 120);
            text("Open Folder", ofbtn_x + (ofbtn_w - tw("Open Folder", 1.6f))/2, ofbtn_y + 10, 1.6f, 180, 200, 255, 255);
            dp_cur = ofbtn_y - 8;

            // ==== REMOVE INSTANCE BUTTON ====
            float rmbtn_y = dp_cur - ofbtn_h;
            bool rm_hov = hit(mouse_x, mouse_y, ofbtn_x, rmbtn_y, ofbtn_w, ofbtn_h);
            rect(ofbtn_x, rmbtn_y, ofbtn_w, ofbtn_h,
                 rm_hov ? (uint8_t)60 : (uint8_t)35,
                 rm_hov ? (uint8_t)25 : (uint8_t)20,
                 rm_hov ? (uint8_t)25 : (uint8_t)20, 220);
            border(ofbtn_x, rmbtn_y, ofbtn_w, ofbtn_h, 1.0f, 180, 60, 60, 120);
            text("Remove Instance", ofbtn_x + (ofbtn_w - tw("Remove Instance", 1.4f))/2, rmbtn_y + 10, 1.4f, 255, 140, 140, 220);
        } else if (cur_bins.empty()) {
            // Empty detail panel message
            text("No instance", PANEL_X + 40, H / 2.0f + 10, 2.0f, 136, 136, 170, 160);
            text("selected", PANEL_X + 60, H / 2.0f - 16, 2.0f, 136, 136, 170, 160);
        }

        // ==== STATUS BAR (bottom 28px) ====
        texquad(tex_bar, 0, 0, W, STATUSBAR_H, 140);
        rect(0, 0, W, STATUSBAR_H, 12, 12, 22, 160);
        rect(0, STATUSBAR_H - 1, W, 1, 70, 130, 230, 40);

        // Auto-launch text
        if (AUTO_LAUNCH_SEC > 0 && state == LauncherState::NORMAL && secs_left > 0) {
            char countdown[64];
            snprintf(countdown, 64, "Auto-launch in %ds...", secs_left);
            text(countdown, 12, 8, 1.2f, 90, 165, 255, 200);
        } else {
            text("Ready", 12, 8, 1.2f, 60, 120, 60, 160);
        }

        // Version + hints
        text("v3.0r", W - 60, 8, 1.0f, 60, 60, 80, 160);
        const char* hint = "ESC cancel | ENTER launch | TAB api";
        text(hint, W / 2 - tw(hint, 0.9f)/2, 8, 0.9f, 60, 60, 80, 130);

        SDL_GL_SwapWindow(win);
        SDL_Delay(16);
    }

    // Cleanup text input if active
    if (state == LauncherState::NAMING_INSTANCE) {
        SDL_StopTextInput(win);
    }

    // Cleanup textures
    GLuint textures[] = {tex_bg, tex_ui_atlas, tex_panel, tex_button, tex_bar, tex_grove};
    glDeleteTextures(6, textures);

    // Cleanup SDL
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);

    // Set selection on the selector so main.cpp can read it
    const auto& final_bins = selector.get_binaries();
    if (!final_bins.empty()) {
        if (bin_sel >= (int)final_bins.size()) bin_sel = 0;
        selector.selected_index = bin_sel;
    }

    std::cout << "[Launcher] API: " << (cfg.graphics_api == GraphicsAPI::VULKAN ? "Vulkan" : "OpenGL")
              << " | Binary: " << cfg.selected_binary
              << (cfg.should_launch ? " | Launching" : " | Cancelled") << std::endl;

    return cfg;
}
