#include "camera_override.h"
#include "mod_tools.h"
#include "../platform/emulator.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <cstdio>
#include <algorithm>

// ============================================================
//  SwordigoDesktop — Modernized Camera Override
//  Dt-based speed, acceleration, smooth mode, scroll zoom,
//  position presets, and better limits.
// ============================================================

// ----- Global state ------------------------------------------
uint32_t g_cam_ctrl_ptr  = 0;
bool     g_cam_active    = false;
CamMode  g_cam_mode      = CamMode::FREE;
float    g_cam_off_x     = 0.0f;
float    g_cam_off_y     = 0.0f;
float    g_cam_off_z     = 0.0f;
bool     g_cam_smooth    = false;   // false = instant, true = smooth interp
float    g_cam_speed_base= 120.0f;  // Units per second at 1x speed

CamPreset g_cam_presets[5] = {};    // All initialized to {0,0,0,false}

// Internal: acceleration tracking
static float s_accel_timer = 0.0f;  // How long movement keys have been held
static const float ACCEL_RAMP = 3.0f;  // Seconds to reach max speed
static const float ACCEL_MAX  = 4.0f;  // Max speed multiplier from acceleration

// Limits
static const float CAM_LIMIT_XZ = 2000.0f;
static const float CAM_LIMIT_Y  = 1000.0f;

// Scroll zoom speed
static const float SCROLL_ZOOM_SPEED = 40.0f;

// ----- Helpers -----------------------------------------------

static inline uint32_t f2u(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Write a Vector3 into guest scratch memory
static uint32_t write_vec3(uint8_t* guest_mem, float x, float y, float z) {
    memcpy(guest_mem + CAM_SCRATCH_VEC3 + 0, &x, 4);
    memcpy(guest_mem + CAM_SCRATCH_VEC3 + 4, &y, 4);
    memcpy(guest_mem + CAM_SCRATCH_VEC3 + 8, &z, 4);
    return CAM_SCRATCH_VEC3;
}

// ----- Public API --------------------------------------------

void cam_capture_controller(uint32_t this_ptr) {
    if (g_cam_ctrl_ptr == 0 && this_ptr != 0) {
        g_cam_ctrl_ptr = this_ptr;
        std::cout << "[Camera] CameraController captured at 0x"
                  << std::hex << this_ptr << std::dec << std::endl;
    }
}

void cam_set_active(bool active) {
    g_cam_active = active;
    s_accel_timer = 0.0f;
    if (!active) {
        g_cam_off_x = g_cam_off_y = g_cam_off_z = 0.0f;
        mod_toast("Camera: Game Control", 1.5f);
        std::cout << "[Camera] Override DISABLED" << std::endl;
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Camera: FREE  [%s]",
                 g_cam_smooth ? "Smooth" : "Instant");
        mod_toast(msg, 2.0f);
        std::cout << "[Camera] Override ENABLED (Free, "
                  << (g_cam_smooth ? "Smooth" : "Instant") << ")" << std::endl;
    }
}

void cam_toggle() {
    cam_set_active(!g_cam_active);
}

void cam_move_scaled(float dx, float dy, float dz, float dt) {
    if (!g_cam_active) return;

    // Acceleration: speed ramps up the longer keys are held
    s_accel_timer += dt;
    float accel = 1.0f + (ACCEL_MAX - 1.0f) * std::min(s_accel_timer / ACCEL_RAMP, 1.0f);

    // Final speed = base * dt * acceleration
    float speed = g_cam_speed_base * dt * accel;

    g_cam_off_x = clampf(g_cam_off_x + dx * speed, -CAM_LIMIT_XZ, CAM_LIMIT_XZ);
    g_cam_off_y = clampf(g_cam_off_y + dy * speed, -CAM_LIMIT_Y,  CAM_LIMIT_Y);
    g_cam_off_z = clampf(g_cam_off_z + dz * speed, -CAM_LIMIT_XZ, CAM_LIMIT_XZ);
}

void cam_scroll_zoom(float delta) {
    if (!g_cam_active) return;
    g_cam_off_z = clampf(g_cam_off_z - delta * SCROLL_ZOOM_SPEED,
                         -CAM_LIMIT_XZ, CAM_LIMIT_XZ);
}

void cam_toggle_smooth() {
    g_cam_smooth = !g_cam_smooth;
    char msg[64];
    snprintf(msg, sizeof(msg), "Camera: %s", g_cam_smooth ? "Smooth" : "Instant");
    mod_toast(msg, 1.5f);
    std::cout << "[Camera] Smooth mode: " << (g_cam_smooth ? "ON" : "OFF") << std::endl;
}

void cam_reset() {
    g_cam_off_x = g_cam_off_y = g_cam_off_z = 0.0f;
    s_accel_timer = 0.0f;
    mod_toast("Camera: Reset", 1.0f);
    std::cout << "[Camera] Position reset" << std::endl;
}

void cam_save_preset(int slot) {
    if (slot < 0 || slot >= 5) return;
    g_cam_presets[slot] = { g_cam_off_x, g_cam_off_y, g_cam_off_z, true };
    char msg[64];
    snprintf(msg, sizeof(msg), "Camera: Saved slot %d", slot + 1);
    mod_toast(msg, 1.0f);
    std::cout << "[Camera] Preset " << slot + 1 << " saved: ("
              << g_cam_off_x << ", " << g_cam_off_y << ", " << g_cam_off_z << ")" << std::endl;
}

void cam_load_preset(int slot) {
    if (slot < 0 || slot >= 5) return;
    if (!g_cam_presets[slot].valid) {
        mod_toast("Camera: Slot empty", 1.0f);
        return;
    }
    g_cam_off_x = g_cam_presets[slot].x;
    g_cam_off_y = g_cam_presets[slot].y;
    g_cam_off_z = g_cam_presets[slot].z;
    char msg[64];
    snprintf(msg, sizeof(msg), "Camera: Loaded slot %d", slot + 1);
    mod_toast(msg, 1.0f);
}

// Reset acceleration when no movement keys are held
// (Called from main.cpp when no cam keys are active)
void cam_reset_accel() {
    s_accel_timer = 0.0f;
}

void cam_apply(Emulator* emu, uint8_t* guest_mem) {
    if (!g_cam_active) return;
    if (g_cam_ctrl_ptr == 0) return;

    uint32_t vec3_addr = write_vec3(guest_mem, g_cam_off_x, g_cam_off_y, g_cam_off_z);
    bool prev_quiet = emu->quiet_mode;
    emu->quiet_mode = true;

    // CameraController::FocusAtPoint(Vector3 const&, bool smooth)
    // r0=this, r1=&vec3, r2=smooth
    emu->call(CaverSym::CameraControllerFocusAtPoint,
              {g_cam_ctrl_ptr, vec3_addr, (uint32_t)(g_cam_smooth ? 1 : 0)});

    emu->quiet_mode = prev_quiet;
}

void cam_debug_string(char* out, int max_len) {
    snprintf(out, max_len,
             "CAM [%s] %s  X:%.1f  Y:%.1f  Z:%.1f  Speed:%.0f  Ctrl:0x%08x",
             g_cam_active ? "ON " : "OFF",
             g_cam_smooth ? "~Smooth" : "Instant",
             g_cam_off_x, g_cam_off_y, g_cam_off_z,
             g_cam_speed_base,
             g_cam_ctrl_ptr);
}
