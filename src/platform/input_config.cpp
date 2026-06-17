// input_config.cpp - Input configuration and virtual button mapping system
// All positions use the 960x544 game coordinate space internally.
// Window-space conversion is done only at render time.

#include "platform/input_config.h"
#include "platform/gl_inc.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <SDL3/SDL.h>

// Game coordinate space constants
static const float GAME_W = 960.0f;
static const float GAME_H = 544.0f;

// Number of segments for circle rendering
static const int CIRCLE_SEGMENTS = 24;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

InputConfig::InputConfig() {
    load_defaults();
}

InputConfig::~InputConfig() {
}

// ---------------------------------------------------------------------------
// Default button layout (matches Vita port)
// ---------------------------------------------------------------------------

void InputConfig::load_defaults() {
    buttons.clear();

    // Helper lambda to create and push a button
    auto add = [this](const char* name, float gx, float gy, float r,
                      int scan, int scan_alt, int tid,
                      int gpad_btn, int gpad_axis = -1) {
        TouchButton b;
        b.name            = name;
        b.game_x          = gx;
        b.game_y          = gy;
        b.radius          = r;
        b.sdl_scancode     = scan;
        b.sdl_scancode_alt = scan_alt;
        b.touch_id        = tid;
        b.gamepad_button  = gpad_btn;
        b.gamepad_axis    = gpad_axis;
        buttons.push_back(b);
    };

    // Movement buttons (left side of screen)
    add("left",  65, 80, 55,
        SDL_SCANCODE_A,     0, 10,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT);

    add("right", 195, 80, 55,
        SDL_SCANCODE_D,     0, 11,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

    // Action buttons (right side of screen)
    add("jump",  860, 80, 45,
        SDL_SCANCODE_SPACE, SDL_SCANCODE_W, 12,
        SDL_GAMEPAD_BUTTON_SOUTH);

    add("attack", 900, 180, 40,
        SDL_SCANCODE_J,     SDL_SCANCODE_Z, 13,
        SDL_GAMEPAD_BUTTON_WEST);

    add("magic", 820, 180, 35,
        SDL_SCANCODE_K,     SDL_SCANCODE_X, 14,
        SDL_GAMEPAD_BUTTON_NORTH);

    // Utility buttons
    add("use_item", 480, 510, 30,
        SDL_SCANCODE_I,     0, 15,
        SDL_GAMEPAD_BUTTON_EAST);

    add("menu", 480, 30, 25,
        SDL_SCANCODE_ESCAPE, 0, 16,
        SDL_GAMEPAD_BUTTON_START);

    add("pause", 920, 510, 25,
        SDL_SCANCODE_P,     0, 17,
        SDL_GAMEPAD_BUTTON_BACK);

    std::cout << "[InputConfig] Loaded " << buttons.size() << " default buttons\n";
}

// ---------------------------------------------------------------------------
// INI Load
// ---------------------------------------------------------------------------

void InputConfig::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "[InputConfig] Config not found at " << path
                  << ", using defaults\n";
        load_defaults();
        return;
    }

    buttons.clear();

    TouchButton current;
    bool have_section = false;

    std::string line;
    while (std::getline(file, line)) {
        // Trim leading/trailing whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Skip comments
        if (line[0] == '#' || line[0] == ';') continue;

        // Section header: [button_name]
        if (line[0] == '[') {
            // Save previous section if we had one
            if (have_section) {
                buttons.push_back(current);
            }
            // Start new section
            current = TouchButton();
            size_t end = line.find(']');
            if (end != std::string::npos) {
                current.name = line.substr(1, end - 1);
            }
            have_section = true;
            continue;
        }

        // Key=value pairs within a section
        if (!have_section) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Trim key and value
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        size_t vs = val.find_first_not_of(" \t");
        if (vs != std::string::npos) val = val.substr(vs);

        // Parse known keys
        if      (key == "game_x")         current.game_x          = std::stof(val);
        else if (key == "game_y")         current.game_y          = std::stof(val);
        else if (key == "radius")         current.radius          = std::stof(val);
        else if (key == "action_type")    current.action_type     = std::stoi(val);
        else if (key == "touch_id")       current.touch_id        = std::stoi(val);
        else if (key == "scancode")       current.sdl_scancode     = std::stoi(val);
        else if (key == "scancode_alt")   current.sdl_scancode_alt = std::stoi(val);
        else if (key == "gamepad_button") current.gamepad_button  = std::stoi(val);
        else if (key == "gamepad_axis")   current.gamepad_axis    = std::stoi(val);
        else if (key == "axis_threshold") current.axis_threshold  = std::stof(val);
        else if (key == "tv_remote_key")  current.tv_remote_key   = std::stoi(val);
    }

    // Push the last section
    if (have_section) {
        buttons.push_back(current);
    }

    file.close();
    std::cout << "[InputConfig] Loaded " << buttons.size()
              << " buttons from " << path << "\n";

    // Fallback: if file was empty or corrupt, restore defaults
    if (buttons.empty()) {
        std::cout << "[InputConfig] Config was empty, loading defaults\n";
        load_defaults();
    }
}

// ---------------------------------------------------------------------------
// INI Save
// ---------------------------------------------------------------------------

void InputConfig::save(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[InputConfig] Failed to save config to " << path << "\n";
        return;
    }

    file << "# Swordigo Desktop - Input Configuration\n";
    file << "# Positions are in 960x544 game coordinate space\n";
    file << "# Scancodes use SDL_Scancode values\n\n";

    for (size_t i = 0; i < buttons.size(); i++) {
        const TouchButton& b = buttons[i];

        file << "[" << b.name << "]\n";
        file << "game_x="         << b.game_x          << "\n";
        file << "game_y="         << b.game_y          << "\n";
        file << "radius="         << b.radius           << "\n";
        file << "action_type="    << b.action_type      << "\n";
        file << "touch_id="       << b.touch_id         << "\n";
        file << "scancode="       << b.sdl_scancode      << "\n";
        file << "scancode_alt="   << b.sdl_scancode_alt  << "\n";
        file << "gamepad_button=" << b.gamepad_button   << "\n";
        file << "gamepad_axis="   << b.gamepad_axis     << "\n";
        file << "axis_threshold=" << b.axis_threshold   << "\n";
        file << "tv_remote_key="  << b.tv_remote_key    << "\n";
        file << "\n";
    }

    file.close();
    std::cout << "[InputConfig] Saved " << buttons.size()
              << " buttons to " << path << "\n";
}

// ---------------------------------------------------------------------------
// Input Lookup
// ---------------------------------------------------------------------------

int InputConfig::find_button_at(float gx, float gy) {
    for (int i = 0; i < (int)buttons.size(); i++) {
        float dx = gx - buttons[i].game_x;
        float dy = gy - buttons[i].game_y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= buttons[i].radius) {
            return i;
        }
    }
    return -1;
}

int InputConfig::find_by_scancode(int scancode) {
    for (int i = 0; i < (int)buttons.size(); i++) {
        if (buttons[i].sdl_scancode == scancode ||
            buttons[i].sdl_scancode_alt == scancode) {
            return i;
        }
    }
    return -1;
}

int InputConfig::find_by_gamepad_button(int button) {
    for (int i = 0; i < (int)buttons.size(); i++) {
        if (buttons[i].gamepad_button == button) {
            return i;
        }
    }
    return -1;
}

int InputConfig::find_by_gamepad_axis(int axis, float value) {
    for (int i = 0; i < (int)buttons.size(); i++) {
        if (buttons[i].gamepad_axis == axis) {
            // Check if the axis value exceeds the threshold in either direction
            if (std::fabs(value) >= buttons[i].axis_threshold) {
                return i;
            }
        }
    }
    return -1;
}

TouchButton* InputConfig::get_button(int index) {
    if (index < 0 || index >= (int)buttons.size()) return nullptr;
    return &buttons[index];
}

// ---------------------------------------------------------------------------
// OpenGL circle drawing helper
// ---------------------------------------------------------------------------

void InputConfig::draw_circle(float cx, float cy, float r,
                              uint8_t red, uint8_t g, uint8_t b, uint8_t a,
                              bool filled) {
    GLenum mode = filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP;

    glColor4ub(red, g, b, a);
    glBegin(mode);

    if (filled) {
        // Center vertex for fan
        glVertex2f(cx, cy);
    }

    for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
        float angle = 2.0f * 3.14159265f * (float)i / (float)CIRCLE_SEGMENTS;
        float px = cx + r * std::cos(angle);
        float py = cy + r * std::sin(angle);
        glVertex2f(px, py);
    }

    glEnd();
}

// ---------------------------------------------------------------------------
// Render touch zone overlay (in-game HUD)
// ---------------------------------------------------------------------------

void InputConfig::render_touch_zones(int win_w, int win_h, bool show_labels) {
    // Scale factors from game space to window space
    float sx = (float)win_w / GAME_W;
    float sy = (float)win_h / GAME_H;

    // Set up 2D orthographic projection for overlay rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, win_w, win_h, 0, -1, 1);  // Top-left origin

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);

    for (int i = 0; i < (int)buttons.size(); i++) {
        const TouchButton& btn = buttons[i];

        // Convert game coords to window coords
        float wx = btn.game_x * sx;
        float wy = btn.game_y * sy;
        float wr = btn.radius * sx;  // Use x-scale for radius (roughly circular)

        // Choose color by button category
        uint8_t r, g, b, a;
        if (btn.name == "left" || btn.name == "right") {
            // Movement: blue
            r = 50; g = 120; b = 255; a = btn.is_pressed ? 120 : 60;
        } else if (btn.name == "menu" || btn.name == "pause") {
            // Menu/system: orange
            r = 255; g = 160; b = 40; a = btn.is_pressed ? 120 : 50;
        } else {
            // Action buttons: green
            r = 60; g = 220; b = 80; a = btn.is_pressed ? 120 : 50;
        }

        // Filled semi-transparent circle
        draw_circle(wx, wy, wr, r, g, b, a, true);

        // Outlined border (slightly brighter)
        draw_circle(wx, wy, wr, r, g, b, (uint8_t)(a + 60), false);
    }

    // Restore GL state
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

// ---------------------------------------------------------------------------
// Render editor overlay (button placement mode)
// ---------------------------------------------------------------------------

void InputConfig::render_editor(int win_w, int win_h,
                                int mouse_x, int mouse_y,
                                bool mouse_pressed) {
    float sx = (float)win_w / GAME_W;
    float sy = (float)win_h / GAME_H;

    glPushAttrib(GL_ALL_ATTRIB_BITS);

    // Set up 2D orthographic projection — bottom-left origin (matches main.cpp mouse coords)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, win_w, 0, win_h, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    // Semi-transparent dark overlay
    glColor4ub(0, 0, 0, 120);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f((float)win_w, 0);
    glVertex2f((float)win_w, (float)win_h);
    glVertex2f(0, (float)win_h);
    glEnd();

    // Convert mouse from GL coords to game coords for hover detection
    float mouse_gx = (float)mouse_x / sx;
    float mouse_gy = (float)mouse_y / sy;
    int hovered = find_button_at(mouse_gx, mouse_gy);

    for (int i = 0; i < (int)buttons.size(); i++) {
        const TouchButton& btn = buttons[i];

        // Convert game coords (Y=0 bottom, matches GL) to window coords
        float wx = btn.game_x * sx;
        float wy = btn.game_y * sy;
        float wr = btn.radius * std::min(sx, sy) * 0.7f;  // Scale down slightly

        // Base color by category
        uint8_t r, g, b;
        if (btn.name == "left" || btn.name == "right") {
            r = 60; g = 130; b = 255;  // Blue for movement
        } else if (btn.name == "menu" || btn.name == "pause") {
            r = 255; g = 160; b = 40;  // Orange for system
        } else {
            r = 60; g = 200; b = 80;   // Green for actions
        }

        // Highlight hovered or dragged button
        uint8_t fill_a = 50;
        uint8_t line_a = 150;
        if (i == hovered || i == dragging_button) {
            fill_a = 100;
            line_a = 255;
        }

        // Filled circle (subtle)
        draw_circle(wx, wy, wr, r, g, b, fill_a, true);

        // Bright outline ring
        draw_circle(wx, wy, wr, r, g, b, line_a, false);

        // Crosshair at center
        float ch = 4.0f;
        glColor4ub(255, 255, 255, 180);
        glBegin(GL_LINES);
        glVertex2f(wx - ch, wy); glVertex2f(wx + ch, wy);
        glVertex2f(wx, wy - ch); glVertex2f(wx, wy + ch);
        glEnd();

        // Button name label below the circle
        // Simple text using quads (crude but functional)
        float label_x = wx - (btn.name.length() * 4.0f);
        float label_y = wy - wr - 14.0f;
        // Draw label background
        float lw = btn.name.length() * 8.0f + 6.0f;
        glColor4ub(0, 0, 0, 180);
        glBegin(GL_QUADS);
        glVertex2f(label_x - 3, label_y - 2);
        glVertex2f(label_x + lw, label_y - 2);
        glVertex2f(label_x + lw, label_y + 10);
        glVertex2f(label_x - 3, label_y + 10);
        glEnd();
        // Draw label text character-by-character using simple pixel font
        glColor4ub(r, g, b, 255);
        glPointSize(1.0f);
        for (size_t c = 0; c < btn.name.size(); c++) {
            float cx = label_x + c * 8.0f;
            // Simple 1-pixel-per-char stub — actual rendering done by GuiRenderer
            glBegin(GL_QUADS);
            glVertex2f(cx, label_y);
            glVertex2f(cx + 6, label_y);
            glVertex2f(cx + 6, label_y + 8);
            glVertex2f(cx, label_y + 8);
            glEnd();
        }
    }

    // Instruction bar at top of screen
    float bar_y = (float)win_h - 30.0f;
    glColor4ub(0, 0, 0, 200);
    glBegin(GL_QUADS);
    glVertex2f(0, bar_y);
    glVertex2f((float)win_w, bar_y);
    glVertex2f((float)win_w, (float)win_h);
    glVertex2f(0, (float)win_h);
    glEnd();

    // Restore GL state
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

// ---------------------------------------------------------------------------
// Editor mouse handlers (all coordinates in game space)
// ---------------------------------------------------------------------------

void InputConfig::editor_mouse_down(float game_x, float game_y) {
    dragging_button = find_button_at(game_x, game_y);
    if (dragging_button >= 0) {
        // Store offset so the button doesn't snap to cursor center
        drag_offset_x = buttons[dragging_button].game_x - game_x;
        drag_offset_y = buttons[dragging_button].game_y - game_y;
        std::cout << "[InputConfig] Editor: grabbed button '"
                  << buttons[dragging_button].name << "'\n";
    }
}

void InputConfig::editor_mouse_move(float game_x, float game_y) {
    if (dragging_button < 0) return;

    // Move the button, clamping to game coordinate bounds
    float new_x = game_x + drag_offset_x;
    float new_y = game_y + drag_offset_y;

    float r = buttons[dragging_button].radius;
    if (new_x - r < 0)      new_x = r;
    if (new_x + r > GAME_W) new_x = GAME_W - r;
    if (new_y - r < 0)      new_y = r;
    if (new_y + r > GAME_H) new_y = GAME_H - r;

    buttons[dragging_button].game_x = new_x;
    buttons[dragging_button].game_y = new_y;
}

void InputConfig::editor_mouse_up() {
    if (dragging_button >= 0) {
        std::cout << "[InputConfig] Editor: released button '"
                  << buttons[dragging_button].name
                  << "' at (" << buttons[dragging_button].game_x
                  << ", " << buttons[dragging_button].game_y << ")\n";
        dragging_button = -1;
    }
}
