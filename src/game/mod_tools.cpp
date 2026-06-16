#include "mod_tools.h"
#include "../platform/gui.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <algorithm>

// ============================================================
//  SwordigoDesktop — Mod Tools Implementation
//  Game speed, pause/step, toast notifications.
// ============================================================

// ---------------------------------------------------------------
// Game Speed
// ---------------------------------------------------------------
float g_game_speed    = 1.0f;
bool  g_game_paused   = false;
bool  g_step_one_frame = false;

static const float SPEED_MIN  = 0.25f;
static const float SPEED_MAX  = 4.0f;
static const float SPEED_STEP = 0.25f;

static char s_speed_label[16] = "1.0x";

static void refresh_speed_label() {
    snprintf(s_speed_label, sizeof(s_speed_label), "%.2gx", g_game_speed);
}

void mod_speed_up() {
    g_game_speed = std::min(g_game_speed + SPEED_STEP, SPEED_MAX);
    refresh_speed_label();
    char msg[64];
    snprintf(msg, sizeof(msg), "Speed: %s", s_speed_label);
    mod_toast(msg, 1.5f);
    std::cout << "[ModTools] " << msg << std::endl;
}

void mod_speed_down() {
    g_game_speed = std::max(g_game_speed - SPEED_STEP, SPEED_MIN);
    refresh_speed_label();
    char msg[64];
    snprintf(msg, sizeof(msg), "Speed: %s", s_speed_label);
    mod_toast(msg, 1.5f);
    std::cout << "[ModTools] " << msg << std::endl;
}

void mod_speed_reset() {
    g_game_speed = 1.0f;
    refresh_speed_label();
    mod_toast("Speed: 1x (Normal)", 1.5f);
    std::cout << "[ModTools] Speed reset to 1.0x" << std::endl;
}

const char* mod_speed_label() {
    return s_speed_label;
}

// ---------------------------------------------------------------
// Pause / Frame Advance
// ---------------------------------------------------------------
void mod_toggle_pause() {
    g_game_paused = !g_game_paused;
    g_step_one_frame = false;
    if (g_game_paused) {
        mod_toast("PAUSED  (F9 = step)", 0.0f); // duration 0 = stays until unpaused
    } else {
        mod_toast("RESUMED", 1.0f);
    }
    std::cout << "[ModTools] " << (g_game_paused ? "PAUSED" : "RESUMED") << std::endl;
}

void mod_step_frame() {
    if (!g_game_paused) return;
    g_step_one_frame = true;
}

// ---------------------------------------------------------------
// Toast Notifications
// ---------------------------------------------------------------
struct Toast {
    std::string message;
    float elapsed;
    float duration;   // 0 = persistent (removed manually)
};

static std::vector<Toast> s_toasts;

void mod_toast(const std::string& message, float duration_sec) {
    // Remove any persistent toast (duration 0) when adding a new one
    s_toasts.erase(
        std::remove_if(s_toasts.begin(), s_toasts.end(),
            [](const Toast& t) { return t.duration <= 0.0f; }),
        s_toasts.end()
    );
    s_toasts.push_back({message, 0.0f, duration_sec});
    // Keep max 5 toasts
    while (s_toasts.size() > 5) {
        s_toasts.erase(s_toasts.begin());
    }
}

// ---------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------
void mod_update(float /*dt*/) {
    // Nothing needed yet — speed/pause are applied in main.cpp
}

// ---------------------------------------------------------------
// Render overlay
// ---------------------------------------------------------------
void mod_render_overlay(GuiRenderer& gui, int win_w, int win_h, float dt) {
    // --- Speed indicator (always visible when not 1.0x) ---
    if (std::fabs(g_game_speed - 1.0f) > 0.01f) {
        char buf[32];
        snprintf(buf, sizeof(buf), "[ %s ]", s_speed_label);
        // Top-right corner
        int x = win_w - 120;
        int y = win_h - 30;
        // Color: green if fast, orange if slow
        int r = g_game_speed < 1.0f ? 255 : 100;
        int g = g_game_speed > 1.0f ? 255 : 180;
        gui.draw_string(buf, x, y, 1.4f, r, g, 80, 255);
    }

    // --- Pause indicator (always visible when paused) ---
    if (g_game_paused) {
        gui.draw_string("|| PAUSED", win_w / 2 - 50, win_h / 2 + 20, 2.0f, 255, 80, 80, 255);
        gui.draw_string("F9 = Step Frame    F8 = Resume", win_w / 2 - 130, win_h / 2 - 10, 1.0f, 200, 200, 200, 255);
    }

    // --- Toast messages (center of screen, stack upward) ---
    int toast_y = win_h / 2 - 40;
    for (auto it = s_toasts.begin(); it != s_toasts.end(); ) {
        if (it->duration > 0.0f) {
            it->elapsed += dt;
            if (it->elapsed >= it->duration) {
                it = s_toasts.erase(it);
                continue;
            }
        }
        // Fade out in the last 0.4 seconds
        int alpha = 255;
        if (it->duration > 0.0f && it->elapsed > it->duration - 0.4f) {
            alpha = (int)(255.0f * (it->duration - it->elapsed) / 0.4f);
            if (alpha < 0) alpha = 0;
        }
        gui.draw_string(it->message.c_str(), win_w / 2 - 80, toast_y, 1.3f, 255, 255, 100, alpha);
        toast_y -= 22;
        ++it;
    }
}
