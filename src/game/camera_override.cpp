#include "camera_override.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <iostream>

// ============================================================
//  SwordigoDesktop — Camera Override Implementation
//  See camera_override.h for architecture notes.
// ============================================================

// ----- Global state ------------------------------------------
uint32_t g_cam_ctrl_ptr = 0;
bool     g_cam_active   = false;
float    g_cam_off_x    = 0.0f;
float    g_cam_off_y    = 0.0f;
float    g_cam_off_z    = 0.0f;

// Limits — prevent camera going too far from the scene
static constexpr float CAM_LIMIT_XZ = 800.0f;
static constexpr float CAM_LIMIT_Y  = 400.0f;

// ----- Internal helpers --------------------------------------

// Pack a float into a uint32_t for passing to emu->call()
static inline uint32_t f2u(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

// Clamp helper
static inline float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Write a Vector3 into guest scratch memory and return its address
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
        std::cout << "[Camera] CameraController captured at guest VA 0x"
                  << std::hex << this_ptr << std::dec << std::endl;
    }
}

void cam_set_active(bool active) {
    g_cam_active = active;
    if (!active) {
        // Reset offsets so the camera snaps back to game control
        g_cam_off_x = g_cam_off_y = g_cam_off_z = 0.0f;
        std::cout << "[Camera] Camera override DISABLED — reset to game control" << std::endl;
    } else {
        std::cout << "[Camera] Camera override ENABLED — use Numpad to pan/zoom" << std::endl;
    }
}

void cam_move(float dx, float dy, float dz) {
    if (!g_cam_active) return;
    g_cam_off_x = clamp(g_cam_off_x + dx, -CAM_LIMIT_XZ, CAM_LIMIT_XZ);
    g_cam_off_y = clamp(g_cam_off_y + dy, -CAM_LIMIT_Y,  CAM_LIMIT_Y);
    g_cam_off_z = clamp(g_cam_off_z + dz, -CAM_LIMIT_XZ, CAM_LIMIT_XZ);
}

void cam_reset() {
    g_cam_off_x = g_cam_off_y = g_cam_off_z = 0.0f;
    std::cout << "[Camera] Camera position reset to origin" << std::endl;
}

void cam_apply(Emulator* emu, uint8_t* guest_mem) {
    if (!g_cam_active) return;
    if (g_cam_ctrl_ptr == 0) return;   // Controller not found yet

    // ---- Strategy A: Direct struct write (preferred once offsets are known) ----
    // When disasm_mini.py gives us the field offsets, uncomment this block
    // and comment out Strategy B below.
    //
    // if (CaverSym::CAM_OFF_X >= 0) {
    //     float* px = (float*)(guest_mem + g_cam_ctrl_ptr + CaverSym::CAM_OFF_X);
    //     float* py = (float*)(guest_mem + g_cam_ctrl_ptr + CaverSym::CAM_OFF_Y);
    //     float* pz = (float*)(guest_mem + g_cam_ctrl_ptr + CaverSym::CAM_OFF_Z);
    //     *px += g_cam_off_x;
    //     *py += g_cam_off_y;
    //     *pz += g_cam_off_z;
    //     return;
    // }

    // ---- Strategy B: Call CameraController::FocusAtPoint(Vector3&, bool=false) ----
    // FocusAtPoint moves the camera focus to a given world position.
    // We pass (0 + our_offset, 0 + our_offset, 0 + our_offset) as target position.
    // This is equivalent to moveCam_C on Android.
    //
    // ARM32 calling convention:
    //   r0 = this  (CameraController*)
    //   r1 = &vec3 (const Vector3&)
    //   r2 = bool  (smooth=false → move immediately)
    //
    // Note: g_cam_off_x/y/z are the ABSOLUTE offsets from the game's natural
    // camera focus. FocusAtPoint sets the absolute focus point, so we need
    // the game's natural focus base. A simpler variant: call with just the delta
    // repeated every frame so the camera keeps the offset.
    // For now we call it with the raw offset values.

    uint32_t vec3_addr = write_vec3(guest_mem, g_cam_off_x, g_cam_off_y, g_cam_off_z);
    bool prev_quiet = emu->quiet_mode;
    emu->quiet_mode = true;

    // CameraController::FocusAtPoint(Vector3 const&, bool)
    // r0=this, r1=&vec3, r2=false(0)
    emu->call(CaverSym::CameraControllerFocusAtPoint,
              {g_cam_ctrl_ptr, vec3_addr, 0});

    emu->quiet_mode = prev_quiet;
}

void cam_debug_string(char* out, int max_len) {
    snprintf(out, max_len,
             "CAM [%s]  X:%.1f  Y:%.1f  Z:%.1f  Ctrl:0x%08x",
             g_cam_active ? "ON " : "OFF",
             g_cam_off_x, g_cam_off_y, g_cam_off_z,
             g_cam_ctrl_ptr);
}
