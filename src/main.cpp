#ifndef _WIN32
#include <unistd.h>
#else
#include <process.h>
#include <windows.h>
#endif
#include <iostream>
#include <iomanip>
#include <vector>
#include <stdint.h>
#include <filesystem>
namespace fs = std::filesystem;
#include "loader/elf_loader.h"
#include "jni/jni_layer.h"
#include "jni/jni_bridge.h"
#include "platform/emulator.h"
#include "platform/display.h"
#include "android/asset_manager.h"
#include "game/camera_override.h"
#include "game/mod_tools.h"
#include "platform/input_config.h"
#include <cstring>
#include <chrono>
#define GL_GLEXT_PROTOTYPES
#include "platform/gl_inc.h"
#include <GL/glext.h>
#include <sys/stat.h>

#include "platform/gui.h"
#include "platform/vulkan_backend.h"
#include "platform/fbo_scaler.h"
#include "platform/io_thread.h"
#include "platform/binary_selector.h"
#include "platform/launcher.h"

extern bool g_display_active;
extern int g_win_w;
extern int g_win_h;
extern int g_draw_w;  // Physical drawable pixels (for glViewport / FBO)
extern int g_draw_h;

// --- Global Context ---
uint8_t* g_guest_memory = nullptr;
const uint32_t GUEST_MEM_SIZE = 0xC0000000; // 3GB

// Game internal render resolution — game engine sees this via setApplicationViewSize.
// Defaults to 1920×1080, but dynamically updated after display init to match
// the display's physical pixel dimensions and native aspect ratio.
// Touch coordinates are kept in legacy 960×544 space and auto-scaled.
static int GAME_W = 1920;
static int GAME_H = 1080;
static float TOUCH_SCALE_X = (float)GAME_W / 960.0f;
// Which game binary to load (switchable via launcher or --lib flag)
static std::string g_lib_name = "libswordigo_nx.so";
std::string g_assets_dir = "assets";  // "assets" for vanilla, "rl_assets" for RLSwordigo
static float TOUCH_SCALE_Y = (float)GAME_H / 544.0f;   // ~1.985

// Add a specific area for guest-side global variables (libc stuff)
const uint32_t GUEST_GLOBALS_BASE = 0x50000; 
const uint32_t GUEST_GLOBALS_SIZE = 0x1000; 

so_module g_main_mod;
ElfLoader* g_loader = nullptr;
JniBridge g_bridge;
Emulator* g_emulator = nullptr;
GuiRenderer g_gui;
InputConfig g_input_config;
SDL_Gamepad* g_gamepad = nullptr;
FBOScale g_fbo_mode = FBOScale::SHARP_BILINEAR;
PostFXState g_postfx;
PostFXPreset g_postfx_preset = PostFXPreset::OFF;

// Graphics API selection (enum defined in vulkan_backend.h)
GraphicsAPI g_graphics_api = GraphicsAPI::OPENGL;
#ifdef VULKAN_BACKEND
VulkanBackend g_vk_backend;
#endif

// Binary selection system
BinarySelector g_binary_selector;

// FWKeyboard API — resolved from game binary symbols
static uint32_t g_fw_sharedKeyboard = 0;   // Caver::FWKeyboard::sharedKeyboard()
static uint32_t g_fw_sendKeyDown = 0;       // Caver::FWKeyboard::SendKeyDownEvent(uint, uint, double)
static uint32_t g_fw_sendKeyUp = 0;         // Caver::FWKeyboard::SendKeyUpEvent(uint, uint, double)
static uint32_t g_fw_sendKeyChar = 0;       // Caver::FWKeyboard::SendKeyCharEvent(uint, double)
static uint32_t g_fw_handleMenuBtn = 0;     // Java_com_touchfoo_swordigo_Native_handleMenuButtonPress
static bool g_typing_mode = false;          // F6 toggle: keyboard sends FWKeyboard events

extern std::string g_save_dir;  // Defined in jni_bridge.cpp
extern bool g_gl_hide_hud;      // Defined in jni_bridge.cpp — hides HUD draw calls
extern int g_death_detected_countdown;  // Defined in jni_bridge.cpp
std::string g_cache_dir;

#include "platform/data_path.h"  // get_data_path() — implemented in data_path.cpp

void call_handle_touch_event(uint32_t addr, uint32_t env, uint32_t obj, int action, int id, double time_val, float x, float y, float old_x, float old_y, int tap_count) {
    if (addr == 0) return;
    
    // Auto-scale from legacy 960×544 touch space to actual game resolution
    x     *= TOUCH_SCALE_X;  y     *= TOUCH_SCALE_Y;
    old_x *= TOUCH_SCALE_X;  old_y *= TOUCH_SCALE_Y;

    // Save current SP
    uint32_t old_sp = g_emulator->get_reg(13);
    
    // Allocate stack space (aligned to 8 bytes)
    uint32_t new_sp = old_sp - 32;
    g_emulator->set_reg(13, new_sp);
    
    // Write stack arguments to guest memory
    uint8_t* memory = g_emulator->get_memory_base();
    *(double*)(memory + new_sp + 0) = time_val;
    *(float*)(memory + new_sp + 8) = x;
    *(float*)(memory + new_sp + 12) = y;
    *(float*)(memory + new_sp + 16) = old_x;
    *(float*)(memory + new_sp + 20) = old_y;
    *(uint32_t*)(memory + new_sp + 24) = tap_count;
    
    // R0-R3 registers
    g_emulator->set_reg(0, env);
    g_emulator->set_reg(1, obj);
    g_emulator->set_reg(2, action);
    g_emulator->set_reg(3, id);
    
    // Run guest function
    g_emulator->run(addr);
    
    // Restore SP
    g_emulator->set_reg(13, old_sp);
}

// Get FWKeyboard singleton pointer from the game engine
static uint32_t get_fw_keyboard() {
    if (!g_fw_sharedKeyboard) return 0;
    g_emulator->run(g_fw_sharedKeyboard);
    return g_emulator->get_reg(0);  // Returns FWKeyboard* in R0
}

// Call FWKeyboard::SendKeyDownEvent(keyCode, modifiers, timestamp) or SendKeyUpEvent
// ARM32 AAPCS: this=R0, keyCode=R1, modifiers=R2, double timestamp → aligned to stack
static void call_fw_key_event(uint32_t func_addr, uint32_t keyboard_ptr, uint32_t keyCode, uint32_t modifiers, double timestamp) {
    if (!func_addr || !keyboard_ptr) return;
    
    uint32_t old_sp = g_emulator->get_reg(13);
    uint32_t new_sp = (old_sp - 16) & ~7;  // 8-byte aligned
    g_emulator->set_reg(13, new_sp);
    
    uint8_t* memory = g_emulator->get_memory_base();
    // double on stack (R3 skipped for 8-byte alignment)
    *(double*)(memory + new_sp) = timestamp;
    
    g_emulator->set_reg(0, keyboard_ptr);  // this
    g_emulator->set_reg(1, keyCode);
    g_emulator->set_reg(2, modifiers);
    // R3 is padding for double alignment
    
    g_emulator->run(func_addr);
    g_emulator->set_reg(13, old_sp);
}

// Call FWKeyboard::SendKeyCharEvent(charCode, timestamp)
// ARM32: this=R0, charCode=R1, double timestamp → R2+R3 (8-byte aligned)
static void call_fw_key_char(uint32_t keyboard_ptr, uint32_t charCode, double timestamp) {
    if (!g_fw_sendKeyChar || !keyboard_ptr) return;
    
    uint32_t old_sp = g_emulator->get_reg(13);
    uint32_t new_sp = (old_sp - 16) & ~7;
    g_emulator->set_reg(13, new_sp);
    
    uint8_t* memory = g_emulator->get_memory_base();
    // charCode in R1, then double must be 8-byte aligned → goes to R2+R3
    union { double d; uint32_t u[2]; } conv;
    conv.d = timestamp;
    
    g_emulator->set_reg(0, keyboard_ptr);
    g_emulator->set_reg(1, charCode);
    g_emulator->set_reg(2, conv.u[0]);  // low word
    g_emulator->set_reg(3, conv.u[1]);  // high word
    
    g_emulator->run(g_fw_sendKeyChar);
    g_emulator->set_reg(13, old_sp);
}

// Send a complete key press (down + up) through FWKeyboard
static void fw_send_key_press(uint32_t keyCode, double timestamp) {
    uint32_t kb = get_fw_keyboard();
    if (!kb) return;
    call_fw_key_event(g_fw_sendKeyDown, kb, keyCode, 0, timestamp);
    call_fw_key_event(g_fw_sendKeyUp, kb, keyCode, 0, timestamp);
}

// Send a character through FWKeyboard (for text input fields)
static void fw_send_char(uint32_t charCode, double timestamp) {
    uint32_t kb = get_fw_keyboard();
    if (!kb) return;
    call_fw_key_char(kb, charCode, timestamp);
}

void init_all() {
    g_guest_memory = new uint8_t[GUEST_MEM_SIZE];
    g_loader = new ElfLoader(g_guest_memory, GUEST_MEM_SIZE);
    g_bridge.init_standard_bridges();
    asset_manager_init(get_data_path(g_assets_dir).c_str());
    g_gui.init();
    g_input_config.load(g_save_dir + "/controls.ini");
    std::cout << "[Input] Loaded controls config (" << g_input_config.button_count() << " buttons)" << std::endl;
    
    // Start async IO thread
    io_thread_start();

    // Initialize FBO Scaler
    if (g_display_active) {
        fbo_init(GAME_W, GAME_H);
    }
    
    // Open any connected gamepad
    SDL_Init(SDL_INIT_GAMEPAD);
    int gpad_count = 0;
    SDL_JoystickID *gpad_ids = SDL_GetGamepads(&gpad_count);
    if (gpad_ids) {
        for (int i = 0; i < gpad_count; i++) {
            g_gamepad = SDL_OpenGamepad(gpad_ids[i]);
            if (g_gamepad) {
                std::cout << "[Input] Gamepad: " << SDL_GetGamepadName(g_gamepad) << std::endl;
                break;
            }
        }
        SDL_free(gpad_ids);
    }
    std::cout << "[Main] Infrastructure initialized." << std::endl;
}

uint32_t setup_jni_env(uint8_t* memory) {
    uint32_t env_ptr = 0x10000;
    uint32_t vtable_ptr = 0x10010;
    
    *(uint32_t*)(memory + env_ptr) = vtable_ptr;
    
    // Fill vtable with a default unhandled bridge address
    for (int i = 0; i < 300; i++) {
        std::string slot_name = "UnhandledJNI_" + std::to_string(i);
        *(uint32_t*)(memory + vtable_ptr + i * 4) = g_bridge.get_address(slot_name);
    }
    
    // Set specific JNI functions at their correct offsets
    *(uint32_t*)(memory + vtable_ptr + 0x18) = g_bridge.get_address("FindClass");
    *(uint32_t*)(memory + vtable_ptr + 0x38) = g_bridge.get_address("ThrowNew");
    *(uint32_t*)(memory + vtable_ptr + 0x4C) = g_bridge.get_address("PushLocalFrame");
    *(uint32_t*)(memory + vtable_ptr + 0x50) = g_bridge.get_address("PopLocalFrame");
    *(uint32_t*)(memory + vtable_ptr + 0x54) = g_bridge.get_address("NewGlobalRef");
    *(uint32_t*)(memory + vtable_ptr + 0x58) = g_bridge.get_address("DeleteGlobalRef");
    *(uint32_t*)(memory + vtable_ptr + 0x5C) = g_bridge.get_address("DeleteLocalRef");
    *(uint32_t*)(memory + vtable_ptr + 0x74) = g_bridge.get_address("NewObjectV");
    *(uint32_t*)(memory + vtable_ptr + 0x7C) = g_bridge.get_address("GetObjectClass");
    *(uint32_t*)(memory + vtable_ptr + 0x84) = g_bridge.get_address("GetMethodID");
    *(uint32_t*)(memory + vtable_ptr + 0x8C) = g_bridge.get_address("CallObjectMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0x98) = g_bridge.get_address("CallBooleanMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0xC8) = g_bridge.get_address("CallIntMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0xD4) = g_bridge.get_address("CallLongMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0xF8) = g_bridge.get_address("CallVoidMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0x178) = g_bridge.get_address("GetFieldID");
    *(uint32_t*)(memory + vtable_ptr + 0x17C) = g_bridge.get_address("GetBooleanField");
    *(uint32_t*)(memory + vtable_ptr + 0x190) = g_bridge.get_address("GetIntField");
    *(uint32_t*)(memory + vtable_ptr + 0x198) = g_bridge.get_address("GetFloatField");
    *(uint32_t*)(memory + vtable_ptr + 0x1C4) = g_bridge.get_address("GetStaticMethodID");
    *(uint32_t*)(memory + vtable_ptr + 0x1CC) = g_bridge.get_address("CallStaticObjectMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0x1D8) = g_bridge.get_address("CallStaticBooleanMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0x208) = g_bridge.get_address("CallStaticIntMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0x21C) = g_bridge.get_address("CallStaticLongMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0x220) = g_bridge.get_address("CallStaticFloatMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0x238) = g_bridge.get_address("CallStaticVoidMethodV");
    *(uint32_t*)(memory + vtable_ptr + 0x240) = g_bridge.get_address("GetStaticFieldID");
    *(uint32_t*)(memory + vtable_ptr + 0x244) = g_bridge.get_address("GetStaticObjectField");
    *(uint32_t*)(memory + vtable_ptr + 0x29C) = g_bridge.get_address("NewStringUTF");
    *(uint32_t*)(memory + vtable_ptr + 0x2A0) = g_bridge.get_address("GetStringUTFLength");
    *(uint32_t*)(memory + vtable_ptr + 0x2A4) = g_bridge.get_address("GetStringUTFChars");
    *(uint32_t*)(memory + vtable_ptr + 0x2A8) = g_bridge.get_address("ReleaseStringUTFChars");
    *(uint32_t*)(memory + vtable_ptr + 0x2AC) = g_bridge.get_address("GetArrayLength");
    *(uint32_t*)(memory + vtable_ptr + 0x2B4) = g_bridge.get_address("GetObjectArrayElement");
    *(uint32_t*)(memory + vtable_ptr + 0x2C0) = g_bridge.get_address("NewByteArray");          // Slot 176
    *(uint32_t*)(memory + vtable_ptr + 0x2CC) = g_bridge.get_address("NewIntArray");
    *(uint32_t*)(memory + vtable_ptr + 0x2E0) = g_bridge.get_address("GetByteArrayElements");  // Slot 184
    *(uint32_t*)(memory + vtable_ptr + 0x2EC) = g_bridge.get_address("GetIntArrayElements");
    *(uint32_t*)(memory + vtable_ptr + 0x300) = g_bridge.get_address("ReleaseByteArrayElements"); // Slot 192
    *(uint32_t*)(memory + vtable_ptr + 0x30C) = g_bridge.get_address("ReleaseIntArrayElements");
    *(uint32_t*)(memory + vtable_ptr + 0x34C) = g_bridge.get_address("SetIntArrayRegion");
    *(uint32_t*)(memory + vtable_ptr + 0x35C) = g_bridge.get_address("RegisterNatives");
    *(uint32_t*)(memory + vtable_ptr + 0x36C) = g_bridge.get_address("GetJavaVM");
    *(uint32_t*)(memory + vtable_ptr + 0x374) = g_bridge.get_address("GetStringUTFRegion");

    // Setup JavaVM structures at 0x11000
    uint32_t vm_ptr = 0x11000;
    uint32_t vm_vtable_ptr = 0x11010;
    *(uint32_t*)(memory + vm_ptr) = vm_vtable_ptr;
    
    for (int i = 0; i < 50; i++) {
        std::string slot_name = "UnhandledVM_" + std::to_string(i);
        *(uint32_t*)(memory + vm_vtable_ptr + i * 4) = g_bridge.get_address(slot_name);
    }
    *(uint32_t*)(memory + vm_vtable_ptr + 0x18) = g_bridge.get_address("GetEnv"); // GetEnv at offset 0x18

    return env_ptr;
}

void load_and_boot() {
    std::string so_path = get_data_path(g_lib_name);
    std::cout << "[Loader] Loading: " << so_path << std::endl;
    uint32_t load_addr = 0x1000000;
    
    // 1. Load ELF
    if (g_loader->load(&g_main_mod, so_path, load_addr) != 0) {
        std::cerr << "[Loader] Failed to load SO" << std::endl;
        exit(1);
    }
    
    // 2. Internal Relocations
    g_loader->relocate(&g_main_mod);
    
    // 3. External Symbol Resolution (to Bridge addresses)
    g_loader->resolve_all_to_bridge(&g_main_mod, &g_bridge, GUEST_GLOBALS_BASE);
    
    std::cout << "[Loader] Game binary ready (all symbols bridged)." << std::endl;

    // Initialize Emulator AFTER memory is prepared
    g_emulator = new Emulator(g_guest_memory, GUEST_MEM_SIZE);
    g_emulator->set_bridge(&g_bridge);

    // Run dynamic initializers (.init_array)
    if (g_main_mod.init_array_vaddr != 0 && g_main_mod.init_array_size > 0) {
        int count = g_main_mod.init_array_size / 4;
        std::cout << "[Boot] Executing " << count << " dynamic initializers..." << std::endl;
        uint32_t* init_array = (uint32_t*)(g_guest_memory + g_main_mod.init_array_vaddr);
        for (int i = 0; i < count; i++) {
            uint32_t init_func = init_array[i];
            if (init_func != 0) {
                if (i % 50 == 0 || i == count - 1) {
                    std::cout << "  -> Initializer " << i << "/" << count << " at 0x" << std::hex << init_func << std::dec << std::endl;
                }
                g_emulator->call(init_func, {});
            }
        }
        std::cout << "[Boot] Dynamic initializers completed successfully." << std::endl;
    }

    // 4. Boot Sequence
    uint32_t setFilesDir = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_setFilesDir");
    uint32_t setCacheDir = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_setCacheDir");
    uint32_t setAssetMgr = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_setAssetManager");
    uint32_t googleSignInCompleted = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_googleSignInCompleted");
    uint32_t handleAppLaunch = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_handleApplicationLaunch");
    uint32_t initMusicPlayer = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_MusicPlayer_initMusicPlayer");
    uint32_t setupNative = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_setupNativeInterface");
    uint32_t setupApp = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_setupApplication");
    uint32_t setApplicationViewSize = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_setApplicationViewSize");
    uint32_t applicationDidBecomeActive = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_applicationDidBecomeActive");

    // Resolve FWKeyboard API symbols (mangled C++ names from the Caver engine)
    g_fw_sharedKeyboard = g_loader->get_symbol_vaddr(&g_main_mod, "_ZN5Caver10FWKeyboard14sharedKeyboardEv");
    g_fw_sendKeyDown    = g_loader->get_symbol_vaddr(&g_main_mod, "_ZN5Caver10FWKeyboard16SendKeyDownEventEjjd");
    g_fw_sendKeyUp      = g_loader->get_symbol_vaddr(&g_main_mod, "_ZN5Caver10FWKeyboard14SendKeyUpEventEjjd");
    g_fw_sendKeyChar    = g_loader->get_symbol_vaddr(&g_main_mod, "_ZN5Caver10FWKeyboard16SendKeyCharEventEjd");
    g_fw_handleMenuBtn  = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_handleMenuButtonPress");
    std::cout << "[Boot] FWKeyboard API: sharedKeyboard=0x" << std::hex << g_fw_sharedKeyboard
              << " sendDown=0x" << g_fw_sendKeyDown << " sendUp=0x" << g_fw_sendKeyUp
              << " sendChar=0x" << g_fw_sendKeyChar << " menuBtn=0x" << g_fw_handleMenuBtn
              << std::dec << std::endl;

    // Setup fake JNI structures in guest memory
    uint32_t env_ptr = setup_jni_env(g_guest_memory);
    std::cout << "[Debug] env_ptr = 0x" << std::hex << env_ptr << " vtable_ptr = 0x" << *(uint32_t*)(g_guest_memory + env_ptr) << std::dec << std::endl;
    std::cout << "[Debug] FindClass slot (0x10028) = 0x" << std::hex << *(uint32_t*)(g_guest_memory + 0x10028) << std::dec << std::endl;
    std::cout << "[Debug] GetStringUTFChars slot (0x102b4) = 0x" << std::hex << *(uint32_t*)(g_guest_memory + 0x102b4) << std::dec << std::endl;


    
    // Allocate some strings in guest memory for paths
    uint32_t path_ptr = 0x20000;
    uint32_t files_dir = path_ptr;
    strcpy((char*)(g_guest_memory + files_dir), g_save_dir.c_str());
    path_ptr += 100;
    
    uint32_t cache_dir = path_ptr;
    strcpy((char*)(g_guest_memory + cache_dir), g_cache_dir.c_str());
    path_ptr += 100;

    if (setFilesDir) {
        std::cout << "[Boot] Calling setFilesDir" << std::endl;
        g_emulator->call(setFilesDir, {env_ptr, 0, files_dir});
    }
    if (setCacheDir) {
        std::cout << "[Boot] Calling setCacheDir" << std::endl;
        g_emulator->call(setCacheDir, {env_ptr, 0, cache_dir});
    }
    if (setAssetMgr) {
        std::cout << "[Boot] Calling setAssetManager" << std::endl;
        g_emulator->call(setAssetMgr, {env_ptr, 0, 0x55555555}); 
    }
    if (googleSignInCompleted) {
        std::cout << "[Boot] Calling googleSignInCompleted" << std::endl;
        g_emulator->call(googleSignInCompleted, {env_ptr, 0, 0});
    }
    if (handleAppLaunch) {
        std::cout << "[Boot] Calling handleApplicationLaunch" << std::endl;
        g_emulator->call(handleAppLaunch, {env_ptr, 0});
    }
    if (initMusicPlayer) {
        std::cout << "[Boot] Calling initMusicPlayer" << std::endl;
        g_emulator->call(initMusicPlayer, {env_ptr, 0x22222222}); // pass non-zero fake MusicPlayer object to enable music callbacks
    }
    if (setupNative) {
        std::cout << "[Boot] Calling setupNativeInterface" << std::endl;
        g_emulator->call(setupNative, {env_ptr, 0});
    }
    if (setupApp) {
        std::cout << "[Boot] Calling setupApplication" << std::endl;
        g_emulator->call(setupApp, {env_ptr, 0});
    }
    if (setApplicationViewSize) {
        std::cout << "[Boot] Calling setApplicationViewSize" << std::endl;
        g_emulator->call(setApplicationViewSize, {env_ptr, 0, (uint32_t)GAME_W, (uint32_t)GAME_H, 1}); // Full HD internal resolution
    }
    if (applicationDidBecomeActive) {
        std::cout << "[Boot] Calling applicationDidBecomeActive" << std::endl;
        g_emulator->call(applicationDidBecomeActive, {env_ptr, 0});
    }

    // ========================================================================
    // Death screen fix: Patch ShowInterstitialAd + wire ad dismissed callback
    // The game calls ShowInterstitialAd() on death, then waits for
    // interstitialAdVisibilityChanged(false) to proceed with respawn.
    // Without this fix, the game hangs forever on the death screen.
    // (The Vita port only patches the ad call but NOT the callback — that's
    // why they still have the bug.)
    // ========================================================================
    {
        // Patch ShowInterstitialAd to return immediately (Thumb: bx lr = 0x4770)
        uint32_t showAd = g_loader->get_symbol_vaddr(&g_main_mod,
            "_ZN5Caver24OnlineController_Android18ShowInterstitialAdERKSsif");
        if (showAd) {
            // Address has Thumb bit set (odd) — strip it for memory access
            uint32_t addr = showAd & ~1u;
            uint8_t* memory = g_emulator->get_memory_base();
            // Write Thumb "bx lr" (0x4770) to make it return immediately
            memory[addr + 0] = 0x70;  // bx lr (Thumb)
            memory[addr + 1] = 0x47;
            std::cout << "[Fix] Patched ShowInterstitialAd at 0x" << std::hex << showAd
                      << " -> bx lr (no-op)" << std::dec << std::endl;
        }

        // Also patch AndroidShowInterstitialAd (wrapper)
        uint32_t showAd2 = g_loader->get_symbol_vaddr(&g_main_mod,
            "_ZN5Caver25AndroidShowInterstitialAdEd");
        if (showAd2) {
            uint32_t addr = showAd2 & ~1u;
            uint8_t* memory = g_emulator->get_memory_base();
            memory[addr + 0] = 0x70;
            memory[addr + 1] = 0x47;
            std::cout << "[Fix] Patched AndroidShowInterstitialAd at 0x" << std::hex << showAd2
                      << " -> bx lr" << std::dec << std::endl;
        }
    }

    // Resolve the ad dismissed callback — called reactively in game loop when death is detected
    uint32_t adVisibilityChanged = g_loader->get_symbol_vaddr(&g_main_mod,
        "Java_com_touchfoo_swordigo_Native_interstitialAdVisibilityChanged");
    if (adVisibilityChanged) {
        std::cout << "[Fix] Resolved interstitialAdVisibilityChanged at 0x"
                  << std::hex << adVisibilityChanged << std::dec
                  << " — will fire reactively on death detection" << std::endl;
    }

    // 5. Game Loop
    uint32_t updateApp = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_updateApplication");
    uint32_t drawApp = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_drawApplication");
    uint32_t handleTouchEvent = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_handleTouchEvent");
    uint32_t snapshotLoaded = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_snapshotLoaded");
    
    // Tell the game that Google Sign-In failed (services not available)
    if (googleSignInCompleted) {
        std::cout << "[Boot] Calling googleSignInCompleted(false) — no Google Play services" << std::endl;
        g_emulator->call(googleSignInCompleted, {env_ptr, 0, 0}); // (env, this, false)
    }

    // NOTE: We used to call handleMenuButtonPress here to hide on-screen controls,
    // but that function opens the game menu AND hides controls. The game then shows
    // controls again on any touch input, making it useless. Instead, we use g_gl_hide_hud
    // in the JNI bridge to make HUD draw calls fully transparent.
    bool g_hide_touch_hud = true;  // Hide touch controls by default (desktop uses keyboard)
    g_gl_hide_hud = g_hide_touch_hud;

    if (updateApp && drawApp) {
        // Real delta time
        Uint64 last_ticks = SDL_GetTicks();
        double accumulated_time = 0.0;
        bool gui_visible = false;  // GUI hidden by default
        bool debug_visible = false;  // F3 debug overlay
        bool hint_shown = false;  // One-time "Press F1" hint
        Uint64 hint_start_time = SDL_GetTicks();  // Show hint for first 5 seconds
        float fps = 0.0f;
        Uint64 fps_last_time = SDL_GetTicks();
        int fps_frame_count = 0;
        
        const int TARGET_FRAMES = g_display_active ? 0 : 1000; // 0 = infinite
        int completed_frames = 0;
        
        // Totals for summary
        uint32_t total_draw_calls = 0;
        uint32_t total_tex_binds = 0;
        uint32_t total_tex_uploads = 0;
        uint32_t total_matrix_ops = 0;
        uint32_t total_state_changes = 0;
        uint32_t total_asset_opens = 0;

        std::cout << "\n========================================" << std::endl;
        if (g_display_active) {
            std::cout << "[Loop] Starting visual game loop (close window to stop)" << std::endl;
        } else {
            std::cout << "[Loop] Starting " << TARGET_FRAMES << "-frame stability test" << std::endl;
        }
        std::cout << "========================================\n" << std::endl;

        // Get the display pointer from main (passed via extern)
        extern Display* g_display_ptr;

        // Input states for touch mapping and GUI integration
        int mouse_x = 0;
        int mouse_y = 0;
        bool mouse_pressed = false;
        bool click_swallowed_by_gui = false;
        float last_mouse_x = -1.0f;
        float last_mouse_y = -1.0f;

        bool key_left = false;
        bool key_right = false;
        bool key_jump = false;
        bool key_attack = false;
        bool key_magic = false;
        bool key_use_item = false;
        bool key_menu = false;

        // Camera override keys (Arrow keys — no numpad needed!)
        bool arrow_left  = false;
        bool arrow_right = false;
        bool arrow_up    = false;
        bool arrow_down  = false;
        bool cam_key_pgup  = false;   // PageUp — raise camera
        bool cam_key_pgdn  = false;   // PageDown — lower camera

        bool running = true;
        while (running) {
            g_frame_stats.reset();
            
            // Real delta time
            Uint64 now_ticks = SDL_GetTicks();
            float dt_seconds = (now_ticks - last_ticks) / 1000.0f;
            if (dt_seconds > 0.1f) dt_seconds = 0.016666668f;
            if (dt_seconds < 0.001f) dt_seconds = 0.016666668f;
            last_ticks = now_ticks;
            accumulated_time += dt_seconds;
            
            // Show details for first 10 frames only
            if (completed_frames < 10) {
                g_emulator->quiet_mode = false;
            } else {
                g_emulator->quiet_mode = true;
            }
            
            // --- Send TOUCH_MOVED every frame for held buttons (uses InputConfig positions) ---
            for (int bi = 0; bi < g_input_config.button_count(); bi++) {
                const TouchButton* btn = g_input_config.get_button(bi);
                if (btn && btn->is_pressed) {
                    call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, btn->touch_id, accumulated_time,
                        btn->game_x, btn->game_y, btn->game_x, btn->game_y, 0);
                }
            }

            // --- Per-frame camera movement (Arrow keys, dt-scaled with acceleration) ---
            if (g_cam_active) {
                bool any_cam_key = arrow_left || arrow_right || arrow_up || arrow_down || cam_key_pgup || cam_key_pgdn;
                if (any_cam_key) {
                    float dx = 0, dy = 0, dz = 0;
                    if (arrow_left)   dx -= 1.0f;
                    if (arrow_right)  dx += 1.0f;
                    if (arrow_up)     dy += 1.0f;
                    if (arrow_down)   dy -= 1.0f;
                    if (cam_key_pgup) dz -= 1.0f;  // zoom in
                    if (cam_key_pgdn) dz += 1.0f;  // zoom out
                    cam_move_scaled(dx, dy, dz, dt_seconds);
                } else {
                    cam_reset_accel();
                }
            }

            // --- Apply game speed multiplier ---
            float game_dt = dt_seconds * g_game_speed;
            uint32_t dt_hex;
            memcpy(&dt_hex, &game_dt, 4);

            // --- Pause / Frame Advance ---
            if (!g_game_paused || g_step_one_frame) {
                g_emulator->call(updateApp, {env_ptr, 0, dt_hex});
                if (g_step_one_frame) {
                    g_step_one_frame = false;
                    mod_toast("Stepped 1 frame", 0.8f);
                }
            }
            
            // --- Death screen workaround: auto-restart on gameover ---
            // The game freezes on death because ShowInterstitialAd waits for
            // a callback that can't be properly delivered. Even the Vita port
            // couldn't fix this. Workaround: detect death via gameover music,
            // wait ~8s for the death animation, then restart the process.
            // The save system auto-checkpoints, so the player resumes from
            // their last checkpoint after restart.
            if (g_death_detected_countdown > 0) {
                g_death_detected_countdown--;
                if (g_death_detected_countdown == 0) {
                    std::cout << "\n[Fix] Death screen detected — auto-restarting engine..." << std::endl;
                    std::cout << "[Fix] You will resume from your last checkpoint." << std::endl;
                    
                    // Clean up SDL before restart
                    SDL_Quit();
                    
                    // Re-exec ourselves using /proc/self/exe
                    // Pass --lib to skip the launcher on restart
                    extern std::string g_lib_name;
                    extern char** g_saved_argv;
                    extern int g_saved_argc;
                    std::vector<std::string> arg_strs;  // Keep strings alive
                    std::vector<char*> args;
                    for (int i = 0; i < g_saved_argc; i++) {
                        std::string a = g_saved_argv[i];
                        // Skip old --lib/--assets and their values
                        if ((a == "--lib" || a == "--assets") && i + 1 < g_saved_argc) { i++; continue; }
                        arg_strs.push_back(a);
                    }
                    arg_strs.push_back("--lib");
                    arg_strs.push_back(g_lib_name);
                    arg_strs.push_back("--assets");
                    arg_strs.push_back(g_assets_dir);
                    for (auto& s : arg_strs) args.push_back(&s[0]);
                    args.push_back(nullptr);
#ifdef _WIN32
                    char exe_path[MAX_PATH];
                    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
                    _spawnv(_P_NOWAIT, exe_path, args.data());
#else
                    execv("/proc/self/exe", args.data());
#endif
                    
                    // If restart fails, just exit
                    std::cerr << "[Fix] Restart failed, please start manually." << std::endl;
                    exit(1);
                }
            }
            
            // Poll async IO thread completion callbacks
            io_thread_poll();

            // Handle async callbacks: if loadSnapshot was called, call snapshotLoaded
            extern bool g_snapshot_load_pending;
            extern bool g_snapshot_has_data;
            extern std::vector<uint8_t> g_snapshot_data;
            if (g_snapshot_load_pending && snapshotLoaded) {
                g_snapshot_load_pending = false;
                if (g_snapshot_has_data && !g_snapshot_data.empty()) {
                    // Allocate byte array in guest memory: [length(4)] [data(N)]
                    static uint32_t save_buf_addr = 0x40000000; // high address for save buffer
                    uint32_t array_len = g_snapshot_data.size();
                    *(uint32_t*)(g_guest_memory + save_buf_addr) = array_len;
                    memcpy(g_guest_memory + save_buf_addr + 4, g_snapshot_data.data(), array_len);
                    std::cout << "[Callback] snapshotLoaded with " << array_len << " bytes of save data" << std::endl;
                    g_emulator->call(snapshotLoaded, {env_ptr, 0, 0, save_buf_addr}); // (env, this, name=null, data=array)
                    g_snapshot_has_data = false;
                } else {
                    std::cout << "[Callback] snapshotLoaded(null, null) — no saved game" << std::endl;
                    g_emulator->call(snapshotLoaded, {env_ptr, 0, 0, 0}); // (env, this, name=null, data=null)
                }
            }
            
            if (g_display_active) {
#ifdef VULKAN_BACKEND
                if (g_graphics_api == GraphicsAPI::VULKAN) {
                    g_vk_backend.begin_frame();
                } else
#endif
                {
                    fbo_begin_game();
                }
            }
            
            g_emulator->call(drawApp, {env_ptr, 0});

            if (g_display_active) {
#ifdef VULKAN_BACKEND
                if (g_graphics_api == GraphicsAPI::VULKAN) {
                    // Vulkan frame end + present is done after overlays
                } else
#endif
                {
                    fbo_end_game_and_blit(g_draw_w, g_draw_h, g_fbo_mode, &g_postfx);
                }
            }

            // Apply camera position override (after draw, before swap)
            cam_apply(g_emulator, g_guest_memory);

            // Render GUI overlay (F1 toggle)
            if (g_display_active && gui_visible) {
                g_gui.render(mouse_x, mouse_y, mouse_pressed, g_win_w, g_win_h);
            }
            
            // Render controls editor overlay (F2 toggle)
            if (g_display_active && g_input_config.is_editing()) {
                g_input_config.render_editor(g_win_w, g_win_h, mouse_x, mouse_y, mouse_pressed);
                // Draw instruction text on top (uses GUI font renderer)
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glViewport(0, 0, g_draw_w, g_draw_h);
                glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
                glOrtho(0, g_win_w, 0, g_win_h, -1, 1);
                glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
                glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                g_gui.draw_string("CONTROLS EDITOR - Drag buttons to reposition | F2 to save & close", 10, g_win_h - 20, 1.5f, 100, 200, 255, 255);
                g_gui.draw_string("Blue=Movement  Green=Action  Orange=Menu", 10, g_win_h - 38, 1.2f, 180, 180, 200, 200);
                glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
                glPopAttrib();
            }
            
            // Render F3 debug overlay
            if (g_display_active && debug_visible) {
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glViewport(0, 0, g_draw_w, g_draw_h);
                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, g_win_w, 0, g_win_h, -1, 1);
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();
                glDisable(GL_TEXTURE_2D);
                glDisable(GL_LIGHTING);
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                // Dark background strip
                glColor4ub(0, 0, 0, 180);
                glBegin(GL_QUADS);
                glVertex2f(10, g_win_h - 10); glVertex2f(420, g_win_h - 10);
                glVertex2f(420, g_win_h - 275); glVertex2f(10, g_win_h - 275);
                glEnd();
                // FPS text using simple pixel rendering
                char dbg[256];
                snprintf(dbg, sizeof(dbg), "FPS: %.1f", fps);
                g_gui.draw_string(dbg, 20, g_win_h - 35, 2.0f, 0, 255, 100, 255);
                snprintf(dbg, sizeof(dbg), "Frame: %d", completed_frames);
                g_gui.draw_string(dbg, 20, g_win_h - 60, 1.4f, 200, 200, 200, 255);
                snprintf(dbg, sizeof(dbg), "Draws: %d  TexBinds: %d", g_frame_stats.draw_calls, g_frame_stats.texture_binds);
                g_gui.draw_string(dbg, 20, g_win_h - 80, 1.2f, 180, 180, 180, 255);
                snprintf(dbg, sizeof(dbg), "Verts: %d  MatOps: %d", g_frame_stats.vertices_submitted, g_frame_stats.matrix_ops);
                g_gui.draw_string(dbg, 20, g_win_h - 97, 1.2f, 180, 180, 180, 255);
                snprintf(dbg, sizeof(dbg), "State: %d  TexUps: %d", g_frame_stats.state_changes, g_frame_stats.tex_uploads);
                g_gui.draw_string(dbg, 20, g_win_h - 114, 1.2f, 180, 180, 180, 255);
                snprintf(dbg, sizeof(dbg), "Render: %dx%d -> Window: %dx%d (Draw: %dx%d)", GAME_W, GAME_H, g_win_w, g_win_h, g_draw_w, g_draw_h);
                g_gui.draw_string(dbg, 20, g_win_h - 131, 1.2f, 140, 140, 160, 255);
                snprintf(dbg, sizeof(dbg), "Mouse: %d,%d  DT: %.4fs", mouse_x, mouse_y, dt_seconds);
                g_gui.draw_string(dbg, 20, g_win_h - 148, 1.2f, 140, 140, 160, 255);
                
                const char* mode_str = "Unknown";
                if (g_fbo_mode == FBOScale::SHARP_BILINEAR) mode_str = "Sharp-Bilinear";
                else if (g_fbo_mode == FBOScale::NEAREST) mode_str = "Nearest";
                else if (g_fbo_mode == FBOScale::CRT_SCANLINE) mode_str = "CRT Scanline";
                snprintf(dbg, sizeof(dbg), "Scale Mode: %s", mode_str);
                g_gui.draw_string(dbg, 20, g_win_h - 165, 1.2f, 255, 200, 100, 255);

                snprintf(dbg, sizeof(dbg), "Speed: %s  %s", mod_speed_label(), g_game_paused ? "|| PAUSED" : "");
                g_gui.draw_string(dbg, 20, g_win_h - 185, 1.2f, 255, 180, 50, 255);
                snprintf(dbg, sizeof(dbg), "F1:GUI F2:Ctrl F3:Dbg F4:Scale F5:Cam F6:PostFX F7:Type F10:HUD");
                g_gui.draw_string(dbg, 20, g_win_h - 202, 1.1f, 100, 100, 120, 200);
                if (g_typing_mode) {
                    g_gui.draw_string("TYPING MODE ACTIVE", 20, g_win_h - 218, 1.2f, 255, 100, 100, 255);
                }
                char cam_dbg[128];
                cam_debug_string(cam_dbg, sizeof(cam_dbg));
                g_gui.draw_string(cam_dbg, 20, g_win_h - 222, 1.1f,
                    g_cam_active ? 0 : 120,
                    g_cam_active ? 220 : 120,
                    g_cam_active ? 100 : 120, 255);

                // Binary info
                const BinaryInfo* binfo = g_binary_selector.get_loaded_info();
                if (binfo) {
                    snprintf(dbg, sizeof(dbg), "Binary: %s (%s)",
                             binfo->filename.c_str(), binfo->label.c_str());
                    g_gui.draw_string(dbg, 20, g_win_h - 240, 1.2f, 100, 200, 255, 255);
                } else {
                    snprintf(dbg, sizeof(dbg), "Binary: %s", g_lib_name.c_str());
                    g_gui.draw_string(dbg, 20, g_win_h - 240, 1.2f, 100, 200, 255, 255);
                }

                // PostFX info
                if (g_postfx.enabled) {
                    snprintf(dbg, sizeof(dbg), "PostFX: %s", g_postfx.preset_name);
                    g_gui.draw_string(dbg, 20, g_win_h - 255, 1.2f, 255, 180, 100, 255);
                } else {
                    g_gui.draw_string("PostFX: Off", 20, g_win_h - 255, 1.2f, 120, 120, 120, 200);
                }

                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
                glPopAttrib();
            }
            
            // Render mod tools overlay (speed indicator, toasts, pause screen)
            if (g_display_active) {
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glViewport(0, 0, g_draw_w, g_draw_h);
                glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
                glOrtho(0, g_win_w, 0, g_win_h, -1, 1);
                glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
                glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                mod_render_overlay(g_gui, g_win_w, g_win_h, dt_seconds);
                glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
                glPopAttrib();
            }

            // Show hint for first 5 seconds
            if (g_display_active && !gui_visible && (SDL_GetTicks() - hint_start_time) < 5000) {
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glViewport(0, 0, g_draw_w, g_draw_h);
                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, g_win_w, 0, g_win_h, -1, 1);
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();
                glDisable(GL_TEXTURE_2D);
                glDisable(GL_LIGHTING);
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4ub(0, 0, 0, 160);
                int cx = g_win_w / 2;
                int cy = g_win_h / 2;
                glBegin(GL_QUADS);
                glVertex2f(cx - 280, cy - 15); glVertex2f(cx + 280, cy - 15);
                glVertex2f(cx + 280, cy + 15); glVertex2f(cx - 280, cy + 15);
                glEnd();
                g_gui.draw_string("WASD:Move  Arrows:Camera  +/-:Speed  F5:Cam  F8:Pause  F12:Fullscreen", cx - 310, cy - 7, 1.1f, 0, 200, 255, 255);
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
                glPopAttrib();
            }
            
            extern int g_gl_diag_frame;
            g_gl_diag_frame++;
            
            // Present the frame
            if (g_display_active && g_display_ptr) {
#ifdef VULKAN_BACKEND
                if (g_graphics_api == GraphicsAPI::VULKAN) {
                    g_vk_backend.end_frame_and_present();
                } else
#endif
                {
                    g_display_ptr->swap();
                }
                
                // Poll and process SDL window and input events
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    switch (event.type) {
                        case SDL_EVENT_QUIT:
                            running = false;
                            break;
                        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                            running = false;
                            break;
                        case SDL_EVENT_WINDOW_RESIZED:
                            g_win_w = event.window.data1;
                            g_win_h = event.window.data2;
                            // Physical drawable size for GL rendering (HiDPI)
                            SDL_GetWindowSizeInPixels(g_display_ptr->get_window(), &g_draw_w, &g_draw_h);
                            std::cout << "[Display] Window resized to " << g_win_w << "x" << g_win_h
                                      << " (drawable: " << g_draw_w << "x" << g_draw_h << ")" << std::endl;
                            break;
                        case SDL_EVENT_MOUSE_BUTTON_DOWN:
                            if (event.button.button == SDL_BUTTON_LEFT) {
                                mouse_pressed = true;
                                mouse_x = event.button.x;
                                mouse_y = g_win_h - event.button.y;
                                
                                // Controls editor intercepts all mouse events
                                if (g_input_config.is_editing()) {
                                    float gx = event.button.x * 960.0f / (float)g_win_w;
                                    float gy = 544.0f - (event.button.y * 544.0f / (float)g_win_h);
                                    g_input_config.editor_mouse_down(gx, gy);
                                    click_swallowed_by_gui = true;
                                    break;
                                }
                                
                                GuiAction gui_action = GUI_NONE;
                                bool gui_consumed = false;
                                if (gui_visible) {
                                    gui_action = g_gui.handle_click(mouse_x, mouse_y, g_win_w, g_win_h);
                                    // Consume click if: action returned, OR click is on menu bar, OR any modal/panel is open
                                    gui_consumed = (gui_action != GUI_NONE) 
                                                || (mouse_y >= g_win_h - (int)(32 * g_gui.get_scale()))
                                                || g_gui.has_modal_open();
                                }
                                if (gui_consumed) {
                                    click_swallowed_by_gui = true;
                                    switch (gui_action) {
                                        case GUI_EXIT:
                                            running = false;
                                            break;
                                        case GUI_PAUSE:
                                            mod_toggle_pause();
                                            std::cout << "[GUI] Emulation " << (g_game_paused ? "PAUSED" : "RESUMED") << std::endl;
                                            break;
                                        case GUI_CUSTOMIZE_CONTROLS:
                                            g_input_config.set_editing(true);
                                            std::cout << "[GUI] Controls editor opened" << std::endl;
                                            break;
                                        case GUI_SAVE_STATE:
                                            std::cout << "[GUI] Save State requested" << std::endl;
                                            break;
                                        case GUI_LOAD_STATE:
                                            std::cout << "[GUI] Load State requested" << std::endl;
                                            break;
                                        case GUI_GAME_SPEED_UP:
                                            mod_speed_up();
                                            break;
                                        case GUI_GAME_SPEED_DOWN:
                                            mod_speed_down();
                                            break;
                                        case GUI_GAME_SPEED_RESET:
                                            mod_speed_reset();
                                            break;
                                        case GUI_TOGGLE_CAM:
                                            cam_toggle();
                                            break;
                                        case GUI_TOGGLE_PAUSE:
                                            mod_toggle_pause();
                                            break;
                                        case GUI_TOGGLE_SMOOTH_CAM:
                                            cam_toggle_smooth();
                                            break;
                                        case GUI_GFX_OPENGL:
                                            g_graphics_api = GraphicsAPI::OPENGL;
                                            std::cout << "[GFX] Graphics API set to OpenGL (active)" << std::endl;
                                            break;
                                        case GUI_GFX_VULKAN:
                                            g_graphics_api = GraphicsAPI::VULKAN;
                                            std::cout << "[GFX] Graphics API set to Vulkan (WIP) — restart required" << std::endl;
                                            break;
                                        // Mod menu actions (value adjustments)
                                        case GUI_MOD_WALK_SPEED_UP:
                                            g_gui.mod_walk_speed = std::min(g_gui.mod_walk_speed + 0.5f, 10.0f);
                                            break;
                                        case GUI_MOD_WALK_SPEED_DOWN:
                                            g_gui.mod_walk_speed = std::max(g_gui.mod_walk_speed - 0.5f, 0.5f);
                                            break;
                                        case GUI_MOD_RUN_SPEED_UP:
                                            g_gui.mod_run_speed = std::min(g_gui.mod_run_speed + 0.5f, 10.0f);
                                            break;
                                        case GUI_MOD_RUN_SPEED_DOWN:
                                            g_gui.mod_run_speed = std::max(g_gui.mod_run_speed - 0.5f, 0.5f);
                                            break;
                                        case GUI_MOD_JUMP_HEIGHT_UP:
                                            g_gui.mod_jump_height = std::min(g_gui.mod_jump_height + 0.5f, 10.0f);
                                            break;
                                        case GUI_MOD_JUMP_HEIGHT_DOWN:
                                            g_gui.mod_jump_height = std::max(g_gui.mod_jump_height - 0.5f, 0.5f);
                                            break;
                                        case GUI_MOD_LEVEL_UP:
                                            g_gui.mod_level = std::min(g_gui.mod_level + 1, 99);
                                            break;
                                        case GUI_MOD_LEVEL_DOWN:
                                            g_gui.mod_level = std::max(g_gui.mod_level - 1, 0);
                                            break;
                                        case GUI_MOD_EXP_UP:
                                            g_gui.mod_exp += 100;
                                            break;
                                        case GUI_MOD_EXP_DOWN:
                                            g_gui.mod_exp = std::max(g_gui.mod_exp - 100, 0);
                                            break;
                                        default:
                                            break;
                                    }
                                } else {
                                    click_swallowed_by_gui = false;
                                    float x = event.button.x * 960.0f / (float)g_win_w;
                                    float y = 544.0f - (event.button.y * 544.0f / (float)g_win_h);
                                    last_mouse_x = x;
                                    last_mouse_y = y;
                                    call_handle_touch_event(handleTouchEvent, env_ptr, 0, 1, 1, accumulated_time, x, y, x, y, 1);
                                }
                            }
                            break;
                        case SDL_EVENT_MOUSE_MOTION:
                            mouse_x = event.motion.x;
                            mouse_y = g_win_h - event.motion.y;
                            if (g_input_config.is_editing() && mouse_pressed) {
                                float gx = event.motion.x * 960.0f / (float)g_win_w;
                                float gy = 544.0f - (event.motion.y * 544.0f / (float)g_win_h);
                                g_input_config.editor_mouse_move(gx, gy);
                            } else if (mouse_pressed && !click_swallowed_by_gui) {
                                float x = event.motion.x * 960.0f / (float)g_win_w;
                                float y = 544.0f - (event.motion.y * 544.0f / (float)g_win_h);
                                call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, 1, accumulated_time, x, y, last_mouse_x, last_mouse_y, 0);
                                last_mouse_x = x;
                                last_mouse_y = y;
                            }
                            break;
                        case SDL_EVENT_MOUSE_BUTTON_UP:
                            if (event.button.button == SDL_BUTTON_LEFT) {
                                mouse_pressed = false;
                                mouse_x = event.button.x;
                                mouse_y = g_win_h - event.button.y;
                                if (g_input_config.is_editing()) {
                                    g_input_config.editor_mouse_up();
                                } else if (!click_swallowed_by_gui) {
                                    float x = event.button.x * 960.0f / (float)g_win_w;
                                    float y = 544.0f - (event.button.y * 544.0f / (float)g_win_h);
                                    call_handle_touch_event(handleTouchEvent, env_ptr, 0, 2, 1, accumulated_time, x, y, last_mouse_x, last_mouse_y, 0);
                                    last_mouse_x = -1.0f;
                                    last_mouse_y = -1.0f;
                                }
                                click_swallowed_by_gui = false;
                            }
                            break;
                        case SDL_EVENT_MOUSE_WHEEL:
                            if (g_cam_active) {
                                cam_scroll_zoom((float)event.wheel.y);
                            }
                            break;
                        case SDL_EVENT_KEY_DOWN:
                            if (event.key.key == SDLK_F1 && !event.key.repeat) {
                                gui_visible = !gui_visible;
                                std::cout << "[GUI] " << (gui_visible ? "ON" : "OFF") << std::endl;
                                break;
                            }
                            if (event.key.key == SDLK_F2 && !event.key.repeat) {
                                g_input_config.toggle_editor();
                                if (!g_input_config.is_editing()) {
                                    g_input_config.save(g_save_dir + "/controls.ini");
                                    std::cout << "[Input] Controls config saved" << std::endl;
                                }
                                std::cout << "[Controls Editor] " << (g_input_config.is_editing() ? "ON" : "OFF") << std::endl;
                                break;
                            }
                            if (event.key.key == SDLK_F3 && !event.key.repeat) {
                                debug_visible = !debug_visible;
                                std::cout << "[Debug] " << (debug_visible ? "ON" : "OFF") << std::endl;
                                break;
                            }
                            if (event.key.key == SDLK_F4 && !event.key.repeat) {
                                g_fbo_mode = static_cast<FBOScale>((static_cast<int>(g_fbo_mode) + 1) % 3);
                                const char* m_name = "Unknown";
                                if (g_fbo_mode == FBOScale::SHARP_BILINEAR) m_name = "Sharp-Bilinear";
                                else if (g_fbo_mode == FBOScale::NEAREST) m_name = "Nearest";
                                else if (g_fbo_mode == FBOScale::CRT_SCANLINE) m_name = "CRT Scanline";
                                std::cout << "[FBO] Scale mode changed to " << m_name << std::endl;
                                break;
                            }
                            if (event.key.key == SDLK_F12 && !event.key.repeat) {
                                SDL_WindowFlags flags = SDL_GetWindowFlags(g_display_ptr->get_window());
                                if (flags & SDL_WINDOW_FULLSCREEN) {
                                    SDL_SetWindowFullscreen(g_display_ptr->get_window(), false);
                                    g_win_w = 1920;
                                    g_win_h = 1080;
                                    SDL_GetWindowSizeInPixels(g_display_ptr->get_window(), &g_draw_w, &g_draw_h);
                                    std::cout << "[Display] Windowed 1920x1080 (drawable: " << g_draw_w << "x" << g_draw_h << ")" << std::endl;
                                } else {
                                    SDL_SetWindowFullscreen(g_display_ptr->get_window(), true);
                                    // Get actual fullscreen resolution (logical + physical)
                                    int fw, fh;
                                    SDL_GetWindowSize(g_display_ptr->get_window(), &fw, &fh);
                                    g_win_w = fw;
                                    g_win_h = fh;
                                    SDL_GetWindowSizeInPixels(g_display_ptr->get_window(), &g_draw_w, &g_draw_h);
                                    std::cout << "[Display] Fullscreen " << fw << "x" << fh << " (drawable: " << g_draw_w << "x" << g_draw_h << ")" << std::endl;
                                }
                                break;
                            }
                            if (event.key.key == SDLK_F5 && !event.key.repeat) {
                                cam_toggle();
                                break;
                            }
                            if (event.key.key == SDLK_F6 && !event.key.repeat) {
                                g_postfx_preset = static_cast<PostFXPreset>(
                                    (static_cast<int>(g_postfx_preset) + 1) % static_cast<int>(PostFXPreset::COUNT)
                                );
                                postfx_apply_preset(g_postfx, g_postfx_preset);
                                std::cout << "[PostFX] Preset: " << g_postfx.preset_name << std::endl;
                                break;
                            }
                            if (event.key.key == SDLK_F7 && !event.key.repeat) {
                                g_typing_mode = !g_typing_mode;
                                if (g_typing_mode) {
                                    SDL_StartTextInput(g_display_ptr->get_window());
                                    std::cout << "[Keyboard] Typing mode ON — keyboard sends FWKeyboard events" << std::endl;
                                } else {
                                    SDL_StopTextInput(g_display_ptr->get_window());
                                    std::cout << "[Keyboard] Typing mode OFF — keyboard sends touch events" << std::endl;
                                }
                                break;
                            }
                            if (event.key.key == SDLK_F10 && !event.key.repeat) {
                                // Toggle host-side touch HUD visibility
                                // (paints over button areas instead of calling handleMenuButtonPress
                                //  which would open the game's settings menu)
                                g_hide_touch_hud = !g_hide_touch_hud;
                                g_gl_hide_hud = g_hide_touch_hud;
                                std::cout << "[Keyboard] Touch HUD: " << (g_hide_touch_hud ? "HIDDEN" : "VISIBLE") << std::endl;
                                break;
                            }
                            if (event.key.key == SDLK_F8 && !event.key.repeat) {
                                mod_toggle_pause();
                                break;
                            }
                            if (event.key.key == SDLK_F9 && !event.key.repeat) {
                                mod_step_frame();
                                break;
                            }
                            if (event.key.key == SDLK_EQUALS && !event.key.repeat) {
                                mod_speed_up();
                                break;
                            }
                            if (event.key.key == SDLK_MINUS && !event.key.repeat) {
                                mod_speed_down();
                                break;
                            }
                            if (event.key.key == SDLK_0 && !event.key.repeat) {
                                mod_speed_reset();
                                break;
                            }
                            // fall through
                        case SDL_EVENT_KEY_UP: {
                            bool is_down = (event.type == SDL_EVENT_KEY_DOWN);
                            int scancode = event.key.scancode;
                            
                            // ---- TYPING MODE: route through FWKeyboard API ----
                            if (g_typing_mode && g_fw_sendKeyDown && g_fw_sendKeyUp) {
                                // Map SDL keysym to FWKeyboard key codes
                                // Based on Caver engine internal codes (iOS/Android keycodes)
                                uint32_t fw_key = 0;
                                switch (event.key.key) {
                                    case SDLK_RETURN:     fw_key = 0x24; break; // Enter
                                    case SDLK_BACKSPACE:  fw_key = 0x33; break; // Backspace/Delete
                                    case SDLK_ESCAPE:     fw_key = 0x35; break; // Escape
                                    case SDLK_LEFT:       fw_key = 0x7B; break; // Arrow left
                                    case SDLK_RIGHT:      fw_key = 0x7C; break; // Arrow right
                                    case SDLK_DOWN:       fw_key = 0x7D; break; // Arrow down
                                    case SDLK_UP:         fw_key = 0x7E; break; // Arrow up
                                    case SDLK_TAB:        fw_key = 0x30; break; // Tab
                                    case SDLK_SPACE:      fw_key = 0x31; break; // Space
                                    case SDLK_DELETE:     fw_key = 0x75; break; // Forward delete
                                    // Menu button (same as handleMenuButtonPress)
                                    case SDLK_F10:        fw_key = 0x5d; break;
                                    default: break;
                                }
                                if (fw_key != 0) {
                                    uint32_t kb = get_fw_keyboard();
                                    if (kb) {
                                        if (is_down)
                                            call_fw_key_event(g_fw_sendKeyDown, kb, fw_key, 0, accumulated_time);
                                        else
                                            call_fw_key_event(g_fw_sendKeyUp, kb, fw_key, 0, accumulated_time);
                                    }
                                }
                                break; // Don't fall through to InputConfig
                            }
                            
                            // ---- NORMAL MODE: route through InputConfig ----
                            int btn_idx = g_input_config.find_by_scancode(scancode);
                            if (btn_idx >= 0) {
                                TouchButton* btn = g_input_config.get_button(btn_idx);
                                if (btn && btn->is_pressed != is_down) {
                                    btn->is_pressed = is_down;
                                    call_handle_touch_event(handleTouchEvent, env_ptr, 0,
                                        is_down ? 1 : 2, btn->touch_id, accumulated_time,
                                        btn->game_x, btn->game_y, btn->game_x, btn->game_y,
                                        is_down ? 1 : 0);
                                }
                                break;
                            }
                            
                            // Camera + arrow keys (camera-only, not game movement)
                            switch (event.key.key) {
                                case SDLK_LEFT:     arrow_left   = is_down; break;
                                case SDLK_RIGHT:    arrow_right  = is_down; break;
                                case SDLK_UP:       arrow_up     = is_down; break;
                                case SDLK_DOWN:     arrow_down   = is_down; break;
                                case SDLK_PAGEUP:   cam_key_pgup = is_down; break;
                                case SDLK_PAGEDOWN: cam_key_pgdn = is_down; break;
                                case SDLK_HOME:
                                    if (is_down) cam_reset();
                                    break;
                            }
                            break;
                        }
                        // --- Text input events (typing mode F7) ---
                        case SDL_EVENT_TEXT_INPUT: {
                            if (g_typing_mode && g_fw_sendKeyChar) {
                                // Send each character through FWKeyboard::SendKeyCharEvent
                                const char* text = event.text.text;
                                for (int ci = 0; text[ci] != '\0'; ci++) {
                                    uint32_t ch = (uint32_t)(unsigned char)text[ci];
                                    fw_send_char(ch, accumulated_time);
                                }
                            }
                            break;
                        }
                        // --- Multi-touch support (touchscreen laptops, up to 10 fingers) ---
                        case SDL_EVENT_FINGER_DOWN: {
                            // SDL finger coords are normalized 0.0-1.0, scale to game 960x544
                            float x = event.tfinger.x * 960.0f;
                            float y = (1.0f - event.tfinger.y) * 544.0f;  // flip Y
                            int finger_id = (int)(event.tfinger.fingerID % 10) + 20;  // IDs 20-29 for touch
                            call_handle_touch_event(handleTouchEvent, env_ptr, 0, 1, finger_id, accumulated_time, x, y, x, y, 1);
                            break;
                        }
                        case SDL_EVENT_FINGER_UP: {
                            float x = event.tfinger.x * 960.0f;
                            float y = (1.0f - event.tfinger.y) * 544.0f;
                            int finger_id = (int)(event.tfinger.fingerID % 10) + 20;
                            call_handle_touch_event(handleTouchEvent, env_ptr, 0, 2, finger_id, accumulated_time, x, y, x, y, 0);
                            break;
                        }
                        case SDL_EVENT_FINGER_MOTION: {
                            float x = event.tfinger.x * 960.0f;
                            float y = (1.0f - event.tfinger.y) * 544.0f;
                            float old_x = (event.tfinger.x - event.tfinger.dx) * 960.0f;
                            float old_y = (1.0f - (event.tfinger.y - event.tfinger.dy)) * 544.0f;
                            int finger_id = (int)(event.tfinger.fingerID % 10) + 20;
                            call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, finger_id, accumulated_time, x, y, old_x, old_y, 0);
                            break;
                        }
                        // --- Gamepad support ---
                        case SDL_EVENT_GAMEPAD_ADDED: {
                            if (!g_gamepad) {
                                g_gamepad = SDL_OpenGamepad(event.gdevice.which);
                                if (g_gamepad)
                                    std::cout << "[Input] Gamepad connected: " << SDL_GetGamepadName(g_gamepad) << std::endl;
                            }
                            break;
                        }
                        case SDL_EVENT_GAMEPAD_REMOVED: {
                            if (g_gamepad && event.gdevice.which == SDL_GetJoystickID(SDL_GetGamepadJoystick(g_gamepad))) {
                                SDL_CloseGamepad(g_gamepad);
                                g_gamepad = nullptr;
                                std::cout << "[Input] Gamepad disconnected" << std::endl;
                            }
                            break;
                        }
                        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
                            if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
                                float val = event.gaxis.value / 32768.0f;
                                if (g_cam_active) {
                                    cam_move_scaled(val, 0.0f, 0.0f, dt_seconds);
                                } else {
                                    bool new_left = (val < -0.2f);
                                    bool new_right = (val > 0.2f);
                                    
                                    if (key_left != new_left) {
                                        key_left = new_left;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0,
                                            key_left ? 1 : 2, 6, accumulated_time,
                                            60.0f, 94.0f, 60.0f, 94.0f, key_left ? 1 : 0);
                                    }
                                    if (key_right != new_right) {
                                        key_right = new_right;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0,
                                            key_right ? 1 : 2, 7, accumulated_time,
                                            155.0f, 94.0f, 155.0f, 94.0f, key_right ? 1 : 0);
                                    }
                                }
                            }
                            break;
                        }
                        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                            bool gp_down = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                            switch (event.gbutton.button) {
                                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
                                    if (key_left != gp_down) {
                                        key_left = gp_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, gp_down ? 1 : 2, 6, accumulated_time, 60.0f, 94.0f, 60.0f, 94.0f, gp_down ? 1 : 0);
                                    }
                                    break;
                                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
                                    if (key_right != gp_down) {
                                        key_right = gp_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, gp_down ? 1 : 2, 7, accumulated_time, 155.0f, 94.0f, 155.0f, 94.0f, gp_down ? 1 : 0);
                                    }
                                    break;
                                case SDL_GAMEPAD_BUTTON_SOUTH:
                                    if (key_jump != gp_down) {
                                        key_jump = gp_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, gp_down ? 1 : 2, 5, accumulated_time, 900.0f, 94.0f, 900.0f, 94.0f, gp_down ? 1 : 0);
                                    }
                                    break;
                                case SDL_GAMEPAD_BUTTON_WEST:
                                    if (key_attack != gp_down) {
                                        key_attack = gp_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, gp_down ? 1 : 2, 8, accumulated_time, 790.0f, 94.0f, 790.0f, 94.0f, gp_down ? 1 : 0);
                                    }
                                    break;
                                case SDL_GAMEPAD_BUTTON_NORTH:
                                    if (key_magic != gp_down) {
                                        key_magic = gp_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, gp_down ? 1 : 2, 10, accumulated_time, 900.0f, 184.0f, 900.0f, 184.0f, gp_down ? 1 : 0);
                                    }
                                    break;
                                case SDL_GAMEPAD_BUTTON_EAST:
                                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
                                case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
                                    if (key_use_item != gp_down) {
                                        key_use_item = gp_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, gp_down ? 1 : 2, 12, accumulated_time, 425.0f, 54.0f, 425.0f, 54.0f, gp_down ? 1 : 0);
                                    }
                                    break;
                                case SDL_GAMEPAD_BUTTON_START:
                                    if (key_menu != gp_down) {
                                        key_menu = gp_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, gp_down ? 1 : 2, 9, accumulated_time, 48.0f, 500.0f, 48.0f, 500.0f, gp_down ? 1 : 0);
                                    }
                                    break;
                            }
                            break;
                        }
                    }
                }
            }
            
            // FPS counter
            fps_frame_count++;
            Uint64 fps_now = SDL_GetTicks();
            if (fps_now - fps_last_time >= 1000) {
                fps = fps_frame_count * 1000.0f / (fps_now - fps_last_time);
                fps_frame_count = 0;
                fps_last_time = fps_now;
            }
            
            completed_frames++;
            
            // Accumulate totals
            total_draw_calls += g_frame_stats.draw_calls;
            total_tex_binds += g_frame_stats.texture_binds;
            total_tex_uploads += g_frame_stats.tex_uploads;
            total_matrix_ops += g_frame_stats.matrix_ops;
            total_state_changes += g_frame_stats.state_changes;
            total_asset_opens += g_frame_stats.asset_opens;
            
            // Print stats periodically
            if (completed_frames == 1 || completed_frames == 10 || 
                completed_frames % 100 == 0) {
                std::cout << "[Frame " << completed_frames << "] "
                          << "draws=" << g_frame_stats.draw_calls
                          << " verts=" << g_frame_stats.vertices_submitted
                          << " tex_binds=" << g_frame_stats.texture_binds
                          << " tex_uploads=" << g_frame_stats.tex_uploads
                          << " vtx_calls=" << g_frame_stats.vertex_pointer_calls
                          << " texc_calls=" << g_frame_stats.texcoord_pointer_calls
                          << " matrix=" << g_frame_stats.matrix_ops
                          << " state=" << g_frame_stats.state_changes
                          << std::endl;
            }
            
            // In headless mode, stop after TARGET_FRAMES
            if (TARGET_FRAMES > 0 && completed_frames >= TARGET_FRAMES) running = false;
        }

        // Cleanup
        g_input_config.save(g_save_dir + "/controls.ini");
        if (g_gamepad) {
            SDL_CloseGamepad(g_gamepad);
            g_gamepad = nullptr;
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "[RESULT] Completed " << completed_frames << " frames" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Total draw calls:      " << total_draw_calls << std::endl;
        std::cout << "  Total tex binds:       " << total_tex_binds << std::endl;
        std::cout << "  Total tex uploads:     " << total_tex_uploads << std::endl;
        std::cout << "  Total matrix ops:      " << total_matrix_ops << std::endl;
        std::cout << "  Total state changes:   " << total_state_changes << std::endl;
        std::cout << "  Total asset opens:     " << total_asset_opens << std::endl;
        std::cout << "  Unique textures seen:  " << g_frame_stats.unique_textures_bound << std::endl;
        std::cout << "  Avg draw/frame:        " << (completed_frames ? total_draw_calls / completed_frames : 0) << std::endl;
        std::cout << "  Avg tex_bind/frame:    " << (completed_frames ? total_tex_binds / completed_frames : 0) << std::endl;
        std::cout << "========================================" << std::endl;
        
        // Asset Proof Summary
        std::cout << "\n--- Asset Proof ---" << std::endl;
        std::cout << "  Menu scene:      " << (g_frame_stats.menu_scene_loaded ? "✓ LOADED" : "✗ missing") << std::endl;
        std::cout << "  Title texture:   " << (g_frame_stats.title_texture_loaded ? "✓ LOADED" : "✗ missing") << std::endl;
        std::cout << "  Common atlas:    " << (g_frame_stats.common_atlas_loaded ? "✓ LOADED" : "✗ missing") << std::endl;
        std::cout << "  Menu background: " << (g_frame_stats.menu_back_loaded ? "✓ LOADED" : "✗ missing") << std::endl;
        std::cout << "-------------------\n" << std::endl;
    }
}




Display* g_display_ptr = nullptr;
char** g_saved_argv = nullptr;
int g_saved_argc = 0;

int main(int argc, char* argv[]) {
    g_saved_argc = argc;
    g_saved_argv = argv;
    // Check for --headless flag
    bool headless = false;
#ifdef HEADLESS_DEFAULT
    headless = true;
#endif
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) headless = true;
#ifdef VULKAN_BACKEND
        if (strcmp(argv[i], "--vulkan") == 0) g_graphics_api = GraphicsAPI::VULKAN;
#endif
        if (strcmp(argv[i], "--test-lib") == 0) {
            g_lib_name = "libswordigo_nx.so";
            std::cout << "[Main] Using libswordigo_nx.so" << std::endl;
        }
        if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
            g_lib_name = argv[++i];
            std::cout << "[Main] Custom lib: " << g_lib_name << std::endl;
        }
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            g_assets_dir = argv[++i];
            std::cout << "[Main] Custom assets: " << g_assets_dir << std::endl;
        }
    }
    
    // Set up user-writable directory paths
#ifdef _WIN32
    std::string appdata = getenv("LOCALAPPDATA") ? getenv("LOCALAPPDATA") : ".";
    std::string base_dir = appdata + "/swordigo-desktop";
#else
    std::string home_dir = getenv("HOME") ? getenv("HOME") : ".";
    std::string xdg_data = getenv("XDG_DATA_HOME") ? getenv("XDG_DATA_HOME") : (home_dir + "/.local/share");
    std::string base_dir = xdg_data + "/swordigo-desktop";
#endif

    fs::create_directories(base_dir + "/save");
    fs::create_directories(base_dir + "/cache");
    g_save_dir = base_dir + "/save";
    g_cache_dir = base_dir + "/cache";
    std::cout << "[Main] Save directory: " << g_save_dir << std::endl;
    std::cout << "[Main] Cache directory: " << g_cache_dir << std::endl;
    
    Display display;
    
    // ========================================================================
    // Binary scan (needed BEFORE launcher so it can show binary list)
    // ========================================================================
    std::string data_dir = ".";
    const char* env_dir = getenv("SWORDIGO_DATA_DIR");
    if (env_dir) data_dir = env_dir;
    std::string registry_path = base_dir + "/swordigo_binaries.json";
    g_binary_selector.scan_directory(data_dir);
    // Also scan base_dir in case .so files are next to the executable (dev mode)
    if (data_dir != "." && data_dir != base_dir) {
        g_binary_selector.scan_directory(base_dir);
    }
    g_binary_selector.load_registry(registry_path);

    // ========================================================================
    // Show unified launcher GUI (graphics API + binary selection)
    // ========================================================================
    if (!headless) {
        bool skip_launcher = false;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--vulkan") == 0 || strcmp(argv[i], "--no-launcher") == 0 ||
                strcmp(argv[i], "--lib") == 0) {
                skip_launcher = true;
                break;
            }
        }
        if (!skip_launcher) {
            LaunchConfig lconf = show_launcher(g_binary_selector);
            if (!lconf.should_launch) {
                std::cout << "[Main] Launch cancelled by user" << std::endl;
                return 0;
            }
            g_graphics_api = lconf.graphics_api;
            g_lib_name = lconf.selected_binary;
            g_assets_dir = lconf.assets_dir;
            g_binary_selector.set_loaded(g_lib_name);
            std::cout << "[Main] Launcher: " 
                      << (g_graphics_api == GraphicsAPI::VULKAN ? "Vulkan" : "OpenGL")
                      << " | Binary: " << g_lib_name
                      << " | Assets: " << g_assets_dir << std::endl;
        }
    }

    if (!headless) {
#ifdef VULKAN_BACKEND
        if (g_graphics_api == GraphicsAPI::VULKAN) {
            if (display.init_vulkan(GAME_W, GAME_H, "Swordigo")) {
                g_display_active = true;
                g_display_ptr = &display;
                if (!g_vk_backend.init(display.get_window(), GAME_W, GAME_H)) {
                    std::cerr << "[Main] Vulkan backend init failed, falling back to OpenGL" << std::endl;
                    g_graphics_api = GraphicsAPI::OPENGL;
                    // Recreate as OpenGL window
                    display = Display();
                    if (display.init(GAME_W, GAME_H, "Swordigo")) {
                        g_display_active = true;
                        g_display_ptr = &display;
                    }
                } else {
                    std::cout << "[Main] Vulkan backend initialized" << std::endl;
                }
            }
        } else
#endif
        if (display.init(1920, 1080, "Swordigo")) {
            g_display_active = true;
            g_display_ptr = &display;
            // Get physical drawable dimensions (HiDPI)
            SDL_GetWindowSizeInPixels(display.get_window(), &g_draw_w, &g_draw_h);

            // --- Dynamic render resolution: match drawable pixels & aspect ratio ---
            // Use the physical drawable size directly as the game render resolution.
            // This gives 1:1 pixel mapping — no upscaling, maximum sharpness.
            // The aspect ratio naturally matches the display (16:10, 16:9, etc.)
            GAME_W = g_draw_w;
            GAME_H = g_draw_h;
            // Clamp to reasonable range: at least 1920 wide, at most 4096 wide
            if (GAME_W < 1920) { GAME_W = 1920; GAME_H = (int)(1920.0f * g_draw_h / g_draw_w); }
            if (GAME_W > 4096) { float s = 4096.0f / GAME_W; GAME_W = 4096; GAME_H = (int)(GAME_H * s); }
            // Ensure even dimensions (GPU-friendly)
            GAME_W &= ~1;
            GAME_H &= ~1;
            // Recompute touch coordinate scaling
            TOUCH_SCALE_X = (float)GAME_W / 960.0f;
            TOUCH_SCALE_Y = (float)GAME_H / 544.0f;

            std::cout << "[Main] Display initialized - logical 1920x1080"
                      << " | drawable: " << g_draw_w << "x" << g_draw_h
                      << " | render: " << GAME_W << "x" << GAME_H
                      << " | aspect: " << std::fixed << std::setprecision(3)
                      << ((float)GAME_W / GAME_H) << std::endl;
        } else {
            std::cerr << "[Main] Display init failed, falling back to headless" << std::endl;
        }
    } else {
        std::cout << "[Main] Running in headless mode" << std::endl;
    }

    // ========================================================================
    // Binary fallback (only if launcher was skipped via CLI flags)
    // ========================================================================
    {
        const auto& bins = g_binary_selector.get_binaries();
        if (g_lib_name != "libswordigo.so") {
            std::cout << "[BinSel] CLI override: " << g_lib_name << std::endl;
        } else if (!bins.empty()) {
            g_lib_name = g_binary_selector.get_default();
            g_binary_selector.set_loaded(g_lib_name);
            std::cout << "[BinSel] Using: " << g_lib_name << std::endl;
        }
        g_binary_selector.save_registry(registry_path);
    }

    init_all();
    load_and_boot();

    // Cleanup FBO/Vulkan and background IO thread
    if (g_display_active) {
#ifdef VULKAN_BACKEND
        if (g_graphics_api == GraphicsAPI::VULKAN) {
            g_vk_backend.destroy();
        } else
#endif
        {
            fbo_destroy();
        }
    }
    io_thread_stop();
    
    std::cout << "[Main] Swordigo — session complete." << std::endl;
    
    return 0;
}

