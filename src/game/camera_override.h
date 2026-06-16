#pragma once
#include <cstdint>
#include <cstring>
#include "platform/emulator.h"

// ============================================================
//  SwordigoDesktop — Camera Override System
//  Ports the "sw3dmod" Android camera-moveable mod logic
//  to the PC port using direct guest-memory access.
//
//  Architecture:
//    - We capture the CameraController 'this' pointer by
//      intercepting its constructor in the Unicorn code hook.
//    - Once captured, we call CameraController::FocusAtPoint
//      (or write directly to camera state) each frame.
//    - SDL keyboard events accumulate deltas into cam_delta.
//    - cam_apply() is called once per frame after drawApp.
//
//  Symbol addresses (from readelf -s libswordigo.so):
//    CameraController::CameraController()  0x002e35c5 (Thumb)
//    CameraController::Update(float)       0x002e36e5 (Thumb)
//    CameraController::FocusAtPoint(...)   0x002e3a25 (Thumb)
//    CameraController::ResetFocus()        0x002e3b75 (Thumb)
//    CameraController::StopFollowing()     0x002e3b85 (Thumb)
//
//  Struct offsets (TBD — fill in after running disasm_mini.py):
//    CAM_OFF_X  ?    (float: horizontal pan)
//    CAM_OFF_Y  ?    (float: vertical height)
//    CAM_OFF_Z  ?    (float: depth/zoom)
// ============================================================

// Known addresses in libswordigo.so (Thumb: real_addr = VA & ~1)
namespace CaverSym {
    // CameraController methods (Thumb addresses — bit0 set means Thumb)
    static constexpr uint32_t CameraControllerCtor   = 0x002e35c5; // C1Ev
    static constexpr uint32_t CameraControllerUpdate = 0x002e36e5; // Update(float)
    static constexpr uint32_t CameraControllerFocusAtPoint  = 0x002e3a25; // FocusAtPoint(Vector3&, bool)
    static constexpr uint32_t CameraControllerResetFocus    = 0x002e3b75; // ResetFocus()
    static constexpr uint32_t CameraControllerStopFollowing = 0x002e3b85; // StopFollowing()
    static constexpr uint32_t CameraControllerGotoTarget    = 0x002e3d3f; // GotoTargetImmediately()

    // Camera (plain Camera class) methods
    static constexpr uint32_t CameraEvaluateViewMatrix = 0x002e34bd; // EvaluateViewMatrix()

    // Struct field offsets within the camera state (TBD — fill after disasm_mini.py)
    // These are byte offsets from the CameraController 'this' pointer (or from Camera*)
    static constexpr int CAM_OFF_X = -1;   // TODO: fill from disassembly
    static constexpr int CAM_OFF_Y = -1;   // TODO: fill from disassembly
    static constexpr int CAM_OFF_Z = -1;   // TODO: fill from disassembly
}

// Scratch area in guest memory for our Vector3 arguments
static constexpr uint32_t CAM_SCRATCH_VEC3 = 0x3FFF0000; // 12 bytes for a Vector3

// ---------------------------------------------------------------
// Camera override state
// ---------------------------------------------------------------
extern uint32_t  g_cam_ctrl_ptr;    // Guest VA of CameraController object (0 = not found yet)
extern bool      g_cam_active;      // Camera override mode enabled
extern float     g_cam_off_x;       // Accumulated X offset
extern float     g_cam_off_y;       // Accumulated Y offset
extern float     g_cam_off_z;       // Accumulated Z offset

// Called from emulator.cpp hook_code when address == CameraController ctor
void cam_capture_controller(uint32_t this_ptr);

// Enable / disable camera override mode
void cam_set_active(bool active);

// Add camera movement delta (called from SDL key events each frame)
void cam_move(float dx, float dy, float dz);

// Reset camera to default position
void cam_reset();

// Called once per frame AFTER drawApp — applies the accumulated delta
// by writing to guest memory directly (if struct offsets known)
// or by calling FocusAtPoint (when struct offsets are TBD).
void cam_apply(Emulator* emu, uint8_t* guest_mem);

// Draw camera overlay info on the F3 debug screen
// Returns a formatted string like "CAM X:+12.3 Y:0.0 Z:-5.0"
void cam_debug_string(char* out, int max_len);
