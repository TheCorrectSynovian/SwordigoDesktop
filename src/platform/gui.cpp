#include "platform/gui.h"
#include <GL/gl.h>
#include <iostream>
#include <cstring>
#include <cstdio>

// External game state for settings panel display
extern bool g_game_paused;
extern bool g_cam_active;
extern bool g_cam_smooth;
extern float g_game_speed;

// Standard 8x8 font bitmap mapping ASCII 32 (' ') to 127
static const uint8_t font_8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00}, // !
    {0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
    {0x36, 0x36, 0x7f, 0x36, 0x7f, 0x36, 0x36, 0x00}, // #
    {0x0c, 0x3f, 0x0c, 0x0e, 0x3c, 0x0c, 0x3e, 0x0c}, // $
    {0x00, 0x66, 0x66, 0x30, 0x18, 0x0c, 0x66, 0x66}, // %
    {0x3c, 0x66, 0x3c, 0x38, 0x67, 0x66, 0x3f, 0x00}, // &
    {0x06, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
    {0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0c}, // (
    {0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x30}, // )
    {0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00}, // *
    {0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00}, // +
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30}, // ,
    {0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00}, // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // .
    {0x00, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00}, // /
    {0x3e, 0x63, 0x67, 0x6f, 0x7b, 0x63, 0x3e, 0x00}, // 0
    {0x0c, 0x1c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3e, 0x00}, // 1
    {0x3e, 0x63, 0x06, 0x1c, 0x30, 0x60, 0x7f, 0x00}, // 2
    {0x7f, 0x06, 0x0c, 0x1c, 0x06, 0x63, 0x3e, 0x00}, // 3
    {0x06, 0x0f, 0x1b, 0x33, 0x7f, 0x03, 0x03, 0x00}, // 4
    {0x7f, 0x60, 0x7e, 0x03, 0x03, 0x63, 0x3e, 0x00}, // 5
    {0x1c, 0x30, 0x60, 0x7e, 0x63, 0x63, 0x3e, 0x00}, // 6
    {0x7f, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x00}, // 7
    {0x3e, 0x63, 0x63, 0x3e, 0x63, 0x63, 0x3e, 0x00}, // 8
    {0x3e, 0x63, 0x63, 0x7f, 0x03, 0x06, 0x3c, 0x00}, // 9
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // :
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00}, // ;
    {0x06, 0x0c, 0x18, 0x30, 0x18, 0x0c, 0x06, 0x00}, // <
    {0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00}, // =
    {0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00}, // >
    {0x3e, 0x63, 0x06, 0x0c, 0x18, 0x00, 0x18, 0x00}, // ?
    {0x3e, 0x63, 0x6f, 0x6b, 0x6b, 0x60, 0x3e, 0x00}, // @
    {0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00}, // A
    {0x7c, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x7c, 0x00}, // B
    {0x3e, 0x63, 0x60, 0x60, 0x60, 0x63, 0x3e, 0x00}, // C
    {0x78, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0x78, 0x00}, // D
    {0x7e, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x7e, 0x00}, // E
    {0x7e, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x00}, // F
    {0x3e, 0x63, 0x60, 0x6e, 0x63, 0x63, 0x3e, 0x00}, // G
    {0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00}, // H
    {0x3e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3e, 0x00}, // I
    {0x1f, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3c, 0x00}, // J
    {0x66, 0x6c, 0x78, 0x70, 0x78, 0x6c, 0x66, 0x00}, // K
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7f, 0x00}, // L
    {0x63, 0x77, 0x7f, 0x6b, 0x63, 0x63, 0x63, 0x00}, // M
    {0x63, 0x63, 0x67, 0x6f, 0x7b, 0x73, 0x63, 0x00}, // N
    {0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00}, // O
    {0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x00}, // P
    {0x3e, 0x63, 0x63, 0x63, 0x6b, 0x66, 0x3d, 0x00}, // Q
    {0x7c, 0x66, 0x66, 0x7c, 0x78, 0x6c, 0x66, 0x00}, // R
    {0x3e, 0x63, 0x38, 0x0e, 0x07, 0x63, 0x3e, 0x00}, // S
    {0x7f, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x00}, // T
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00}, // U
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00}, // V
    {0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00}, // W
    {0x63, 0x63, 0x36, 0x1c, 0x36, 0x63, 0x63, 0x00}, // X
    {0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x00}, // Y
    {0x7f, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x7f, 0x00}, // Z
    {0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00}, // [
    {0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x00}, // backslash
    {0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00}, // ]
    {0x08, 0x1c, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // ^
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff}, // _
    {0x18, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00}, // `
    {0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x3b, 0x00}, // a
    {0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x7c, 0x00}, // b
    {0x00, 0x00, 0x3c, 0x60, 0x60, 0x62, 0x3c, 0x00}, // c
    {0x06, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00}, // d
    {0x00, 0x00, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00}, // e
    {0x0e, 0x18, 0x3e, 0x18, 0x18, 0x18, 0x18, 0x00}, // f
    {0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x3c}, // g
    {0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00}, // h
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00}, // i
    {0x06, 0x00, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x3c}, // j
    {0x60, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0x00}, // k
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00}, // l
    {0x00, 0x00, 0x66, 0x7f, 0x6b, 0x6b, 0x63, 0x00}, // m
    {0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00}, // n
    {0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x00}, // o
    {0x00, 0x00, 0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60}, // p
    {0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x06}, // q
    {0x00, 0x00, 0x7c, 0x66, 0x60, 0x60, 0x60, 0x00}, // r
    {0x00, 0x00, 0x3e, 0x60, 0x3c, 0x06, 0x7c, 0x00}, // s
    {0x18, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x0d, 0x06}, // t
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3b, 0x00}, // u
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00}, // v
    {0x00, 0x00, 0x63, 0x6b, 0x6b, 0x7f, 0x36, 0x00}, // w
    {0x00, 0x00, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x00}, // x
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x3c}, // y
    {0x00, 0x00, 0x7e, 0x0c, 0x18, 0x30, 0x7e, 0x00}, // z
    {0x0c, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0c, 0x00}, // {
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // |
    {0x30, 0x18, 0x18, 0x0c, 0x18, 0x18, 0x30, 0x00}, // }
    {0x3b, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ~
    {0x1c, 0x36, 0x36, 0x1c, 0x00, 0x00, 0x00, 0x00}  // copyright/bullet
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

GuiRenderer::GuiRenderer() {}
GuiRenderer::~GuiRenderer() {}

// ============================================================================
// init() — Build the menu structure
// ============================================================================

void GuiRenderer::init() {
    menus.clear();
    active_menu = -1;
    hover_menu = -1;
    hover_item = -1;
    paused = false;
    show_about = false;

    float s = gui_scale;
    float text_scale = 1.5f * s;
    float char_w = 8.0f * text_scale;
    float pad = (float)BASE_PAD * s;
    float cur_x = pad;

    // --- File menu ---
    {
        Menu m;
        m.title = "File";
        m.items.push_back(MenuItem("Save State", GUI_SAVE_STATE));
        m.items.push_back(MenuItem("Load State", GUI_LOAD_STATE));
        m.items.push_back(MenuItem("---", GUI_NONE, false));
        m.items.push_back(MenuItem("Exit Game", GUI_EXIT));
        m.x = cur_x;
        m.w = (float)m.title.length() * char_w + pad * 2.0f;
        cur_x += m.w + pad;
        menus.push_back(m);
    }

    // --- Emulation menu ---
    {
        Menu m;
        m.title = "Emulation";
        m.items.push_back(MenuItem("Pause", GUI_PAUSE));
        m.items.push_back(MenuItem("Restart Engine", GUI_RESTART));
        m.x = cur_x;
        m.w = (float)m.title.length() * char_w + pad * 2.0f;
        cur_x += m.w + pad;
        menus.push_back(m);
    }

    // --- Config menu ---
    {
        Menu m;
        m.title = "Config";
        m.items.push_back(MenuItem("Customize Controls", GUI_CUSTOMIZE_CONTROLS));
        m.items.push_back(MenuItem("Audio Settings", GUI_AUDIO_SETTINGS));
        m.items.push_back(MenuItem("Toggle VSync", GUI_TOGGLE_VSYNC));
        m.x = cur_x;
        m.w = (float)m.title.length() * char_w + pad * 2.0f;
        cur_x += m.w + pad;
        menus.push_back(m);
    }

    // --- Settings menu (for mod tools) ---
    {
        Menu m;
        m.title = "Settings";
        m.items.push_back(MenuItem("GUI Scale +", GUI_SCALE_UP));
        m.items.push_back(MenuItem("GUI Scale -", GUI_SCALE_DOWN));
        m.items.push_back(MenuItem("---", GUI_NONE, false));
        m.items.push_back(MenuItem("Speed Up  (+)", GUI_GAME_SPEED_UP));
        m.items.push_back(MenuItem("Speed Down (-)", GUI_GAME_SPEED_DOWN));
        m.items.push_back(MenuItem("Speed Reset (0)", GUI_GAME_SPEED_RESET));
        m.items.push_back(MenuItem("---", GUI_NONE, false));
        m.items.push_back(MenuItem("Toggle Camera (F5)", GUI_TOGGLE_CAM));
        m.items.push_back(MenuItem("Pause Game (F8)", GUI_TOGGLE_PAUSE));
        m.x = cur_x;
        m.w = (float)m.title.length() * char_w + pad * 2.0f;
        cur_x += m.w + pad;
        menus.push_back(m);
    }

    // --- Help menu ---
    {
        Menu m;
        m.title = "Help";
        m.items.push_back(MenuItem("Keybinds", GUI_KEYBINDS));
        m.items.push_back(MenuItem("About", GUI_ABOUT));
        m.x = cur_x;
        m.w = (float)m.title.length() * char_w + pad * 2.0f;
        cur_x += m.w + pad;
        menus.push_back(m);
    }
}

// ============================================================================
// Drawing primitives
// ============================================================================

void GuiRenderer::draw_rect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    glColor4ub(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void GuiRenderer::draw_border(float x, float y, float w, float h, float thickness, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    glColor4ub(r, g, b, a);
    glLineWidth(thickness);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void GuiRenderer::draw_char(char c, float x, float y, float scale, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int idx = (uint8_t)c - 32;
    if (idx < 0 || idx >= 96) return;
    const uint8_t* glyph = font_8x8[idx];

    glColor4ub(r, g, b, a);
    glBegin(GL_QUADS);
    for (int row = 0; row < 8; ++row) {
        uint8_t row_data = glyph[row];
        for (int col = 0; col < 8; ++col) {
            if (row_data & (1 << (7 - col))) {
                float px = x + col * scale;
                float py = y + (7 - row) * scale;
                glVertex2f(px, py);
                glVertex2f(px + scale, py);
                glVertex2f(px + scale, py + scale);
                glVertex2f(px, py + scale);
            }
        }
    }
    glEnd();
}

void GuiRenderer::draw_string(const std::string& str, float x, float y, float scale, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    float cur_x = x;
    for (char c : str) {
        if (c == '\n') {
            y -= 12.0f * scale;
            cur_x = x;
            continue;
        }
        draw_char(c, cur_x, y, scale, r, g, b, a);
        cur_x += 8.0f * scale;
    }
}

// ============================================================================
// render() — Draw the full menu bar, dropdowns, and modals
// ============================================================================

void GuiRenderer::render(int mouse_x, int mouse_y, bool mouse_click, int win_w, int win_h) {
    // Save current OpenGL states
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    // Set viewport to full window
    glViewport(0, 0, win_w, win_h);

    // Set 2D orthographic projection (origin bottom-left)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, win_w, 0, win_h, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Disable elements that conflict with GUI rendering
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float s = gui_scale;
    float fwin_w = (float)win_w;
    float fwin_h = (float)win_h;
    float bar_h = (float)BASE_BAR_H * s;
    float bar_y = fwin_h - bar_h;
    float pad = (float)BASE_PAD * s;
    float item_h = (float)BASE_ITEM_H * s;

    // ---- Update hover state from mouse position ----
    // NOTE: mouse_y is already in GL coordinates (bottom-up), passed by caller
    hover_menu = -1;
    hover_item = -1;

    // Check menu title hover
    if (mouse_y >= (int)bar_y && mouse_y < win_h) {
        for (int i = 0; i < (int)menus.size(); ++i) {
            if (mouse_x >= (int)menus[i].x && mouse_x < (int)(menus[i].x + menus[i].w)) {
                hover_menu = i;
                break;
            }
        }
    }

    // Check dropdown item hover
    if (active_menu >= 0 && active_menu < (int)menus.size()) {
        const Menu& am = menus[active_menu];
        float dd_x = am.x;
        float dd_top = bar_y;  // dropdown starts just below the bar
        // Calculate dropdown width from widest item
        float dd_w = am.w;
        float dd_text_scale = 1.3f * s;
        float dd_char_w = 8.0f * dd_text_scale;
        for (size_t j = 0; j < am.items.size(); ++j) {
            float item_w = (float)am.items[j].label.length() * dd_char_w + pad * 4.0f;
            if (item_w > dd_w) dd_w = item_w;
        }
        float dd_h = (float)am.items.size() * item_h;
        float dd_bottom = dd_top - dd_h;

        if (mouse_x >= (int)dd_x && mouse_x < (int)(dd_x + dd_w) &&
            mouse_y >= (int)dd_bottom && mouse_y < (int)dd_top) {
            // Which item?
            int item_idx = (int)((dd_top - (float)mouse_y) / item_h);
            if (item_idx >= 0 && item_idx < (int)am.items.size()) {
                hover_item = item_idx;
            }
        }
    }

    // ---- Update pause/resume label dynamically ----
    for (size_t i = 0; i < menus.size(); ++i) {
        for (size_t j = 0; j < menus[i].items.size(); ++j) {
            if (menus[i].items[j].action == GUI_PAUSE) {
                menus[i].items[j].label = g_game_paused ? "Resume" : "Pause";
            }
        }
    }

    // ================================================================
    // 1. Menu bar background
    // ================================================================
    draw_rect(0, bar_y, fwin_w, bar_h, 15, 15, 25, 235);
    // Subtle bottom border on the bar
    draw_rect(0, bar_y, fwin_w, 1.0f, 60, 60, 80, 200);

    // ================================================================
    // 2. Menu titles
    // ================================================================
    float title_scale = 1.5f * s;
    float title_char_h = 8.0f * title_scale;
    float title_text_y = bar_y + (bar_h - title_char_h) / 2.0f;

    for (int i = 0; i < (int)menus.size(); ++i) {
        const Menu& m = menus[i];
        bool is_hovered = (hover_menu == i);
        bool is_active = (active_menu == i);

        // Background highlight for hovered/active title
        if (is_active) {
            draw_rect(m.x, bar_y, m.w, bar_h, 50, 50, 70, 255);
        } else if (is_hovered) {
            draw_rect(m.x, bar_y, m.w, bar_h, 50, 50, 70, 255);
        }

        // Title text color
        uint8_t tr, tg, tb, ta;
        if (is_active) {
            tr = 70; tg = 130; tb = 230; ta = 255;  // accent
        } else {
            tr = 180; tg = 180; tb = 200; ta = 255;  // normal
        }

        float text_x = m.x + pad;
        draw_string(m.title, text_x, title_text_y, title_scale, tr, tg, tb, ta);
    }

    // ================================================================
    // 3. Active dropdown panel
    // ================================================================
    if (active_menu >= 0 && active_menu < (int)menus.size()) {
        const Menu& am = menus[active_menu];
        float dd_x = am.x;
        float dd_top = bar_y;

        // Calculate dropdown width from widest item
        float item_text_scale = 1.3f * s;
        float item_char_w = 8.0f * item_text_scale;
        float dd_w = am.w;
        for (size_t j = 0; j < am.items.size(); ++j) {
            float iw2 = (float)am.items[j].label.length() * item_char_w + pad * 4.0f;
            if (iw2 > dd_w) dd_w = iw2;
        }
        float dd_h = (float)am.items.size() * item_h;
        float dd_bottom = dd_top - dd_h;

        // Dropdown background
        draw_rect(dd_x, dd_bottom, dd_w, dd_h, 25, 25, 40, 245);
        // Dropdown border
        draw_border(dd_x, dd_bottom, dd_w, dd_h, 1.0f, 60, 60, 80, 200);

        // Draw each item
        for (int j = 0; j < (int)am.items.size(); ++j) {
            const MenuItem& mi = am.items[j];
            float iy = dd_top - (float)(j + 1) * item_h;

            // Separator
            if (mi.label == "---") {
                float sep_y = iy + item_h / 2.0f;
                draw_rect(dd_x + 4.0f, sep_y, dd_w - 8.0f, 1.0f, 60, 60, 80, 200);
                continue;
            }

            // Hover highlight
            if (hover_item == j && mi.enabled) {
                draw_rect(dd_x + 1.0f, iy + 1.0f, dd_w - 2.0f, item_h - 2.0f, 50, 60, 90, 255);
            }

            // Item text
            uint8_t ir, ig, ib, ia;
            if (!mi.enabled) {
                ir = 80; ig = 80; ib = 100; ia = 255;  // disabled
            } else {
                ir = 200; ig = 200; ib = 220; ia = 255;  // normal
            }

            float item_text_y = iy + (item_h - 8.0f * item_text_scale) / 2.0f;
            draw_string(mi.label, dd_x + pad * 2.0f, item_text_y, item_text_scale, ir, ig, ib, ia);
        }
    }

    // ================================================================
    // 4. About modal
    // ================================================================
    if (show_about) {
        // Full-screen dim overlay
        draw_rect(0, 0, fwin_w, fwin_h, 0, 0, 0, 180);

        float modal_w = 640.0f;
        float modal_h = 560.0f;
        float mx = (fwin_w - modal_w) / 2.0f;
        float my = (fwin_h - modal_h) / 2.0f;

        // Modal background with subtle gradient effect (two layers)
        draw_rect(mx, my, modal_w, modal_h, 15, 15, 28, 252);
        draw_rect(mx, my + modal_h - 60.0f, modal_w, 60.0f, 25, 30, 55, 252);
        draw_border(mx, my, modal_w, modal_h, 2.0f, 70, 130, 230, 255);

        // Title
        float ty = my + modal_h - 40.0f;
        draw_string("Swordigo Desktop", mx + 20.0f, ty, 2.2f, 70, 180, 255, 255);
        draw_string("Remastered", mx + 330.0f, ty, 1.6f, 180, 130, 255, 255);

        // Separator
        float sep = ty - 14.0f;
        draw_rect(mx + 20.0f, sep, modal_w - 40.0f, 2.0f, 70, 130, 230, 200);

        float y = sep - 24.0f;
        float ls = 1.3f; // label scale
        float ts = 1.2f; // text scale
        float line = 16.0f;

        // --- Core Team ---
        draw_string("CORE TEAM", mx + 20.0f, y, 1.5f, 255, 200, 80, 255);
        y -= line + 4.0f;
        draw_string("Lead Developer", mx + 30.0f, y, ts, 100, 180, 255, 255);
        draw_string("TheMegineBraine", mx + 230.0f, y, ts, 220, 220, 240, 255);
        y -= line;
        draw_string("Developer", mx + 30.0f, y, ts, 100, 180, 255, 255);
        draw_string("TheCorrectSynovian", mx + 230.0f, y, ts, 220, 220, 240, 255);
        y -= line + 8.0f;

        // Separator
        draw_rect(mx + 20.0f, y, modal_w - 40.0f, 1.0f, 50, 50, 70, 180);
        y -= 16.0f;

        // --- Research & Community ---
        draw_string("RESEARCH & COMMUNITY", mx + 20.0f, y, 1.5f, 255, 200, 80, 255);
        y -= line + 4.0f;
        draw_string("SwMini Mod Loader", mx + 30.0f, y, ts, 100, 220, 140, 255);
        y -= line;
        draw_string("  ItsJustSomeDude", mx + 30.0f, y, ts, 220, 220, 240, 255);
        draw_string("Reverse engineering, Lua expansion", mx + 250.0f, y, 1.0f, 150, 150, 170, 220);
        y -= line;
        draw_string("  Kiziyon", mx + 30.0f, y, ts, 220, 220, 240, 255);
        draw_string("Reverse engineering, FWKeyboard discovery", mx + 250.0f, y, 1.0f, 150, 150, 170, 220);
        y -= line + 4.0f;
        draw_string("Swordigo Vita Port", mx + 30.0f, y, ts, 100, 220, 140, 255);
        y -= line;
        draw_string("  Rinnegatamante", mx + 30.0f, y, ts, 220, 220, 240, 255);
        draw_string("ARM-to-desktop porting, VitaGL bridge", mx + 250.0f, y, 1.0f, 150, 150, 170, 220);
        y -= line + 8.0f;

        // Separator
        draw_rect(mx + 20.0f, y, modal_w - 40.0f, 1.0f, 50, 50, 70, 180);
        y -= 16.0f;

        // --- Original Game ---
        draw_string("ORIGINAL GAME", mx + 20.0f, y, 1.5f, 255, 200, 80, 255);
        y -= line + 4.0f;
        draw_string("Swordigo (C) 2012 Ville Makynen / Touch Foo", mx + 30.0f, y, ts, 200, 200, 220, 255);
        y -= line;
        draw_string("All Rights Reserved - touchfoo.com/swordigo", mx + 30.0f, y, ts, 150, 150, 170, 200);
        y -= line + 8.0f;

        // Separator
        draw_rect(mx + 20.0f, y, modal_w - 40.0f, 1.0f, 50, 50, 70, 180);
        y -= 16.0f;

        // --- Dependencies ---
        draw_string("POWERED BY", mx + 20.0f, y, 1.5f, 255, 200, 80, 255);
        y -= line + 4.0f;
        draw_string("Unicorn Engine (GPL-2.0)", mx + 30.0f, y, ts, 180, 180, 200, 255);
        draw_string("ARMv7 emulation", mx + 380.0f, y, 1.0f, 130, 130, 150, 200);
        y -= line;
        draw_string("SDL2 (Zlib)", mx + 30.0f, y, ts, 180, 180, 200, 255);
        draw_string("Window + Input", mx + 380.0f, y, 1.0f, 130, 130, 150, 200);
        y -= line;
        draw_string("OpenAL Soft (LGPL-2.1)", mx + 30.0f, y, ts, 180, 180, 200, 255);
        draw_string("Audio playback", mx + 380.0f, y, 1.0f, 130, 130, 150, 200);
        y -= line;
        draw_string("GlossHook by XMDS (MIT)", mx + 30.0f, y, ts, 180, 180, 200, 255);
        draw_string("SwMini reference", mx + 380.0f, y, 1.0f, 130, 130, 150, 200);
        y -= line + 10.0f;

        // Footer
        draw_rect(mx + 20.0f, y, modal_w - 40.0f, 1.0f, 50, 50, 70, 180);
        y -= 18.0f;
        draw_string("License: MIT  |  Built with love for game preservation", mx + 30.0f, y, 1.1f, 120, 120, 150, 200);

        // Close button (bottom right)
        float close_w = 110.0f;
        float close_h = 30.0f;
        float close_x = mx + modal_w - close_w - 20.0f;
        float close_y = my + 15.0f;
        bool close_hover = (mouse_x >= (int)close_x && mouse_x < (int)(close_x + close_w) &&
                            mouse_y >= (int)close_y && mouse_y < (int)(close_y + close_h));
        draw_rect(close_x, close_y, close_w, close_h,
                  close_hover ? (uint8_t)50 : (uint8_t)35,
                  close_hover ? (uint8_t)70 : (uint8_t)35,
                  close_hover ? (uint8_t)120 : (uint8_t)55,
                  255);
        draw_border(close_x, close_y, close_w, close_h, 1.0f, 70, 130, 230, 255);
        float close_text_w = 5.0f * 8.0f * 1.4f;
        draw_string("Close", close_x + (close_w - close_text_w) / 2.0f,
                     close_y + (close_h - 8.0f * 1.4f) / 2.0f, 1.4f, 200, 200, 220, 255);
    }

    // ================================================================
    // 5. Settings panel (central modal)
    // ================================================================
    if (show_settings) {
        draw_rect(0, 0, fwin_w, fwin_h, 0, 0, 0, 160);

        float pw = 480.0f * s;
        float ph = 520.0f * s;
        float spx = (fwin_w - pw) / 2.0f;
        float spy = (fwin_h - ph) / 2.0f;

        draw_rect(spx, spy, pw, ph, 18, 20, 32, 248);
        draw_border(spx, spy, pw, ph, 2.0f, 70, 130, 230, 255);

        float ts = 1.3f * s;
        float rh = 26.0f * s;
        float mg = 22.0f * s;
        float cx = spx + mg;
        float cy = spy + ph - 38.0f * s;
        float cw = pw - mg * 2.0f;
        float box_sz = 13.0f * s;

        // Title
        draw_string("Settings", cx, cy, 2.0f * s, 70, 130, 230, 255);
        cy -= 14.0f * s;
        draw_rect(cx, cy, cw, 1.0f, 50, 50, 70, 200);
        cy -= rh * 0.6f;

        // Helper lambdas
        auto draw_section = [&](const char* label) {
            draw_string(label, cx, cy, ts * 0.85f, 90, 160, 240, 220);
            cy -= rh;
        };

        auto draw_checkbox = [&](const char* label, bool checked, bool enabled) {
            float bx = cx + 4*s, by = cy + 3*s;
            uint8_t bc = enabled ? 120 : 55;
            draw_border(bx, by, box_sz, box_sz, 1.5f, bc, bc, bc+20, 200);
            if (checked) {
                uint8_t fr = enabled ? 50 : 40, fg = enabled ? 200 : 60, fb = enabled ? 90 : 50;
                draw_rect(bx+2.5f*s, by+2.5f*s, box_sz-5*s, box_sz-5*s, fr, fg, fb, 255);
            }
            uint8_t lr = enabled ? 200 : 90, lg = enabled ? 200 : 90, lb = enabled ? 220 : 100;
            draw_string(label, bx + box_sz + 10*s, cy + 3*s, ts * 0.9f, lr, lg, lb, 255);
            if (!enabled) {
                draw_string("(soon)", cx + cw - 55*s, cy + 3*s, ts * 0.65f, 70, 70, 90, 140);
            }
            cy -= rh;
        };

        auto draw_value = [&](const char* label, const char* val, bool enabled) {
            uint8_t lr = enabled ? 200 : 90, lg = enabled ? 200 : 90, lb = enabled ? 220 : 100;
            draw_string(label, cx + 4*s, cy + 3*s, ts * 0.9f, lr, lg, lb, 255);
            float vx = cx + cw * 0.55f;
            if (enabled) {
                draw_string("<", vx, cy + 3*s, ts * 0.9f, 70, 130, 230, 255);
                draw_string(val, vx + 14*s, cy + 3*s, ts * 0.9f, 255, 220, 100, 255);
                float vw2 = strlen(val) * 8.0f * ts * 0.9f;
                draw_string(">", vx + 14*s + vw2 + 6*s, cy + 3*s, ts * 0.9f, 70, 130, 230, 255);
            } else {
                draw_string(val, vx, cy + 3*s, ts * 0.9f, 70, 70, 90, 140);
                draw_string("(soon)", cx + cw - 55*s, cy + 3*s, ts * 0.65f, 70, 70, 90, 140);
            }
            cy -= rh;
        };

        // ── Display ──
        draw_section("Display");
        draw_checkbox("Show FPS Overlay", false, false);
        draw_checkbox("VSync", false, false);
        char sc_buf[16]; snprintf(sc_buf, 16, "%.0f%%", gui_scale * 100.0f);
        draw_value("GUI Scale", sc_buf, true);

        // ── Game ──
        draw_section("Game");
        draw_checkbox("Pause Game", g_game_paused, true);
        char sp_buf[16]; snprintf(sp_buf, 16, "%.1fx", g_game_speed);
        draw_value("Game Speed", sp_buf, true);

        // ── Camera ──
        draw_section("Camera");
        draw_checkbox("Free Camera", g_cam_active, true);
        draw_checkbox("Smooth Mode", g_cam_smooth, true);

        // ── Mod Tools ──
        draw_section("Mod Tools (Coming Soon)");
        draw_checkbox("God Mode", false, false);
        draw_checkbox("Infinite Mana", false, false);
        draw_checkbox("Fly Mode", false, false);
        draw_value("Walk Speed", "1.0x", false);
        draw_value("Jump Height", "1.0x", false);

        // Close button
        float cl_w = 90.0f * s, cl_h = 28.0f * s;
        float cl_x = spx + pw - cl_w - mg, cl_y = spy + 12.0f * s;
        bool cl_hov = (mouse_x >= (int)cl_x && mouse_x < (int)(cl_x+cl_w) &&
                       mouse_y >= (int)cl_y && mouse_y < (int)(cl_y+cl_h));
        draw_rect(cl_x, cl_y, cl_w, cl_h, cl_hov ? 50 : 30, cl_hov ? 55 : 30, cl_hov ? 80 : 45, 255);
        draw_border(cl_x, cl_y, cl_w, cl_h, 1.0f, 70, 130, 230, 255);
        float cts = ts * 0.9f;
        draw_string("Close", cl_x + (cl_w - 5*8*cts)/2, cl_y + (cl_h - 8*cts)/2, cts, 200, 200, 220, 255);
    }

    // ================================================================
    // 5. Title text on the right side of the menu bar
    // ================================================================
    {
        float title_label_scale = 1.4f * s;
        float title_label_w = 16.0f * 8.0f * title_label_scale;  // "Swordigo Desktop" = 16 chars
        float title_label_x = fwin_w - title_label_w - 12.0f;
        float title_label_y = bar_y + (bar_h - 8.0f * title_label_scale) / 2.0f;
        draw_string("Swordigo Desktop", title_label_x, title_label_y, title_label_scale,
                     100, 180, 255, 200);
    }

    // ================================================================
    // Restore OpenGL state
    // ================================================================
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

// ============================================================================
// handle_click() — Process a mouse click, return action if applicable
// ============================================================================

GuiAction GuiRenderer::handle_click(int mouse_x, int mouse_y, int win_w, int win_h) {
    float bar_h = (float)BASE_BAR_H * gui_scale;
    float bar_y = (float)win_h - bar_h;
    float pad = (float)BASE_PAD * gui_scale;
    float item_h = (float)BASE_ITEM_H * gui_scale;
    float s = gui_scale;

    // ---- Settings panel click handling ----
    if (show_settings) {
        float pw = 480.0f * s;
        float ph = 520.0f * s;
        float spx = ((float)win_w - pw) / 2.0f;
        float spy = ((float)win_h - ph) / 2.0f;
        float mg = 22.0f * s;
        float rh = 26.0f * s;
        float cw = pw - mg * 2.0f;

        // Close button
        float cl_w = 90.0f * s, cl_h = 28.0f * s;
        float cl_x = spx + pw - cl_w - mg, cl_y = spy + 12.0f * s;
        if (mouse_x >= (int)cl_x && mouse_x < (int)(cl_x+cl_w) &&
            mouse_y >= (int)cl_y && mouse_y < (int)(cl_y+cl_h)) {
            show_settings = false;
            return GUI_NONE;
        }

        // Click inside panel?
        if (mouse_x >= (int)spx && mouse_x < (int)(spx+pw) &&
            mouse_y >= (int)spy && mouse_y < (int)(spy+ph)) {

            // Compute which row was clicked
            float first_row = spy + ph - 38.0f*s - 14.0f*s - rh*0.6f;
            // Row layout: header, item, item, item, header, item, item, header, item, item, header, item, item, item, item, item
            // Rows: 0=Display(hdr), 1=FPS(dis), 2=VSync(dis), 3=Scale(val),
            //       4=Game(hdr), 5=Pause(cb), 6=Speed(val),
            //       7=Camera(hdr), 8=FreeCam(cb), 9=Smooth(cb),
            //       10=Mod(hdr), 11=God(dis), 12=Mana(dis), 13=Fly(dis), 14=Walk(dis), 15=Jump(dis)
            int row = (int)((first_row - (float)mouse_y) / rh);

            // Value row: check if click is on left (<) or right (>) arrow
            float vx = spx + mg + cw * 0.55f;
            bool click_left = (mouse_x < (int)(vx + 14*s));
            bool click_right = !click_left;

            switch (row) {
                case 3: // GUI Scale
                    if (click_left) { scale_down(); init(); }
                    else { scale_up(); init(); }
                    return GUI_NONE;
                case 5: // Pause Game
                    return GUI_TOGGLE_PAUSE;
                case 6: // Game Speed
                    return click_left ? GUI_GAME_SPEED_DOWN : GUI_GAME_SPEED_UP;
                case 8: // Free Camera
                    return GUI_TOGGLE_CAM;
                case 9: // Smooth Mode
                    return GUI_TOGGLE_SMOOTH_CAM;
                default:
                    break;
            }
            return GUI_NONE; // clicked on disabled/header row
        }

        // Click outside panel — close it
        show_settings = false;
        active_menu = -1;
        return GUI_NONE;
    }

    // ---- About modal close button ----
    if (show_about) {
        float fwin_w = (float)win_w;
        float fwin_h = (float)win_h;
        float modal_w = 640.0f;
        float modal_h = 560.0f;
        float mx = (fwin_w - modal_w) / 2.0f;
        float my = (fwin_h - modal_h) / 2.0f;

        float close_w = 110.0f;
        float close_h = 30.0f;
        float close_x = mx + modal_w - close_w - 20.0f;
        float close_y = my + 15.0f;

        if (mouse_x >= (int)close_x && mouse_x < (int)(close_x + close_w) &&
            mouse_y >= (int)close_y && mouse_y < (int)(close_y + close_h)) {
            show_about = false;
            return GUI_NONE;
        }
        if (mouse_x >= (int)mx && mouse_x < (int)(mx + modal_w) &&
            mouse_y >= (int)my && mouse_y < (int)(my + modal_h)) {
            return GUI_NONE;
        }
        show_about = false;
        active_menu = -1;
        return GUI_NONE;
    }

    // ---- Click on a dropdown item ----
    if (active_menu >= 0 && active_menu < (int)menus.size()) {
        const Menu& am = menus[active_menu];
        float dd_x = am.x;
        float dd_top = bar_y;

        float hc_text_scale = 1.3f * gui_scale;
        float hc_char_w = 8.0f * hc_text_scale;
        float dd_w = am.w;
        for (size_t j = 0; j < am.items.size(); ++j) {
            float iw2 = (float)am.items[j].label.length() * hc_char_w + pad * 4.0f;
            if (iw2 > dd_w) dd_w = iw2;
        }
        float dd_h = (float)am.items.size() * item_h;
        float dd_bottom = dd_top - dd_h;

        if (mouse_x >= (int)dd_x && mouse_x < (int)(dd_x + dd_w) &&
            mouse_y >= (int)dd_bottom && mouse_y < (int)dd_top) {
            int item_idx = (int)((dd_top - (float)mouse_y) / item_h);
            if (item_idx >= 0 && item_idx < (int)am.items.size()) {
                const MenuItem& mi = am.items[item_idx];
                if (mi.enabled && mi.label != "---") {
                    GuiAction action = mi.action;
                    active_menu = -1;

                    // Handle special actions inline
                    if (action == GUI_PAUSE) {
                        return GUI_PAUSE;  // let main.cpp handle toggle
                    }
                    if (action == GUI_ABOUT) {
                        show_about = true;
                        return GUI_NONE;
                    }
                    if (action == GUI_SCALE_UP) {
                        scale_up();
                        init();
                        return GUI_NONE;
                    }
                    if (action == GUI_SCALE_DOWN) {
                        scale_down();
                        init();
                        return GUI_NONE;
                    }

                    return action;
                }
            }
            return GUI_NONE;
        }
    }

    // ---- Click on menu title bar ----
    if (mouse_y >= (int)bar_y && mouse_y < win_h) {
        for (int i = 0; i < (int)menus.size(); ++i) {
            if (mouse_x >= (int)menus[i].x && mouse_x < (int)(menus[i].x + menus[i].w)) {
                // Settings title opens the settings panel
                if (menus[i].title == "Settings") {
                    show_settings = true;
                    active_menu = -1;
                    return GUI_NONE;
                }
                if (active_menu == i) {
                    active_menu = -1;
                } else {
                    active_menu = i;
                }
                return GUI_NONE;
            }
        }
        active_menu = -1;
        return GUI_NONE;
    }

    // ---- Click elsewhere ----
    if (active_menu >= 0) {
        active_menu = -1;
        return GUI_NONE;
    }

    return GUI_NONE;
}
