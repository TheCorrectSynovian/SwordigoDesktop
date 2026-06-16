#include <iostream>
#include <vector>
#include <stdint.h>
#include "loader/elf_loader.h"
#include "jni/jni_layer.h"
#include "jni/jni_bridge.h"
#include "platform/emulator.h"
#include "platform/display.h"
#include "android/asset_manager.h"
#include "game/camera_override.h"
#include <cstring>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <sys/stat.h>

#include "platform/gui.h"

extern bool g_display_active;
extern int g_win_w;
extern int g_win_h;

// --- Global Context ---
uint8_t* g_guest_memory = nullptr;
const uint32_t GUEST_MEM_SIZE = 0xC0000000; // 3GB

// Add a specific area for guest-side global variables (libc stuff)
const uint32_t GUEST_GLOBALS_BASE = 0x50000; 
const uint32_t GUEST_GLOBALS_SIZE = 0x1000; 

so_module g_main_mod;
ElfLoader* g_loader = nullptr;
JniBridge g_bridge;
Emulator* g_emulator = nullptr;
GuiRenderer g_gui;

extern std::string g_save_dir;  // Defined in jni_bridge.cpp
std::string g_cache_dir;

std::string get_data_path(const std::string& relative_path) {
    const char* env_dir = getenv("SWORDIGO_DATA_DIR");
    std::string base_dir;
    if (env_dir) {
        base_dir = env_dir;
    } else {
#ifdef SWORDIGO_DATA_DIR
        base_dir = SWORDIGO_DATA_DIR;
#else
        base_dir = "./";
#endif
    }
    // Ensure base_dir ends with a slash if not empty
    if (!base_dir.empty() && base_dir.back() != '/') {
        base_dir += "/";
    }
    return base_dir + relative_path;
}

void call_handle_touch_event(uint32_t addr, uint32_t env, uint32_t obj, int action, int id, double time_val, float x, float y, float old_x, float old_y, int tap_count) {
    if (addr == 0) return;
    
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

void init_all() {
    g_guest_memory = new uint8_t[GUEST_MEM_SIZE];
    g_loader = new ElfLoader(g_guest_memory, GUEST_MEM_SIZE);
    g_bridge.init_standard_bridges();
    asset_manager_init(get_data_path("assets").c_str());
    g_gui.init();
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
    std::string so_path = get_data_path("libswordigo.so");
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
        g_emulator->call(setApplicationViewSize, {env_ptr, 0, 960, 544, 1}); // Match Vita port: 960x544
    }
    if (applicationDidBecomeActive) {
        std::cout << "[Boot] Calling applicationDidBecomeActive" << std::endl;
        g_emulator->call(applicationDidBecomeActive, {env_ptr, 0});
    }

    // 5. Game Loop — 1000-frame stability test
    uint32_t updateApp = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_updateApplication");
    uint32_t drawApp = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_drawApplication");
    uint32_t handleTouchEvent = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_handleTouchEvent");
    uint32_t snapshotLoaded = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_snapshotLoaded");
    
    // Tell the game that Google Sign-In failed (services not available)
    if (googleSignInCompleted) {
        std::cout << "[Boot] Calling googleSignInCompleted(false) — no Google Play services" << std::endl;
        g_emulator->call(googleSignInCompleted, {env_ptr, 0, 0}); // (env, this, false)
    }

    if (updateApp && drawApp) {
        // Real delta time
        Uint32 last_ticks = SDL_GetTicks();
        double accumulated_time = 0.0;
        bool gui_visible = false;  // GUI hidden by default
        bool debug_visible = false;  // F3 debug overlay
        bool hint_shown = false;  // One-time "Press F1" hint
        Uint32 hint_start_time = SDL_GetTicks();  // Show hint for first 5 seconds
        float fps = 0.0f;
        Uint32 fps_last_time = SDL_GetTicks();
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

        // Camera override keys (Numpad)
        bool cam_key_left  = false;   // KP4 — pan left
        bool cam_key_right = false;   // KP6 — pan right
        bool cam_key_fwd   = false;   // KP8 — move forward (−Z)
        bool cam_key_back  = false;   // KP2 — move backward (+Z)
        bool cam_key_up    = false;   // KP7 / PageUp — raise camera
        bool cam_key_down  = false;   // KP1 / PageDown — lower camera
        bool cam_key_zin   = false;   // KP+ — zoom in
        bool cam_key_zout  = false;   // KP− — zoom out
        const float CAM_SPEED = 5.0f;
        const float CAM_ZOOM  = 10.0f;

        bool running = true;
        while (running) {
            g_frame_stats.reset();
            
            // Real delta time
            Uint32 now_ticks = SDL_GetTicks();
            float dt_seconds = (now_ticks - last_ticks) / 1000.0f;
            if (dt_seconds > 0.1f) dt_seconds = 0.016666668f;
            if (dt_seconds < 0.001f) dt_seconds = 0.016666668f;
            last_ticks = now_ticks;
            accumulated_time += dt_seconds;
            uint32_t dt_hex;
            memcpy(&dt_hex, &dt_seconds, 4);
            
            // Show details for first 10 frames only
            if (completed_frames < 10) {
                g_emulator->quiet_mode = false;
            } else {
                g_emulator->quiet_mode = true;
            }
            
            // --- Send TOUCH_MOVED every frame for held keys (Vita exact coords 960x544) ---
            if (key_left)
                call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, 6, accumulated_time, 60.0f, 94.0f, 60.0f, 94.0f, 0);
            if (key_right)
                call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, 7, accumulated_time, 155.0f, 94.0f, 155.0f, 94.0f, 0);
            if (key_jump)
                call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, 5, accumulated_time, 900.0f, 94.0f, 900.0f, 94.0f, 0);
            if (key_attack)
                call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, 8, accumulated_time, 790.0f, 94.0f, 790.0f, 94.0f, 0);
            if (key_magic)
                call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, 10, accumulated_time, 900.0f, 184.0f, 900.0f, 184.0f, 0);

            // --- Per-frame camera accumulation (held keys) ---
            if (g_cam_active) {
                if (cam_key_left)  cam_move(-CAM_SPEED, 0.0f,  0.0f);
                if (cam_key_right) cam_move(+CAM_SPEED, 0.0f,  0.0f);
                if (cam_key_fwd)   cam_move(0.0f,  0.0f, -CAM_SPEED);
                if (cam_key_back)  cam_move(0.0f,  0.0f, +CAM_SPEED);
                if (cam_key_up)    cam_move(0.0f, +CAM_SPEED,  0.0f);
                if (cam_key_down)  cam_move(0.0f, -CAM_SPEED,  0.0f);
                if (cam_key_zin)   cam_move(0.0f,  0.0f, -CAM_ZOOM);
                if (cam_key_zout)  cam_move(0.0f,  0.0f, +CAM_ZOOM);
            }

            g_emulator->call(updateApp, {env_ptr, 0, dt_hex});
            
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
            
            g_emulator->call(drawApp, {env_ptr, 0});

            // Apply camera position override (after draw, before swap)
            cam_apply(g_emulator, g_guest_memory);

            // Render GUI overlay (F1 toggle)
            if (g_display_active && gui_visible) {
                g_gui.render(mouse_x, mouse_y, mouse_pressed);
            }
            
            // Render F3 debug overlay
            if (g_display_active && debug_visible) {
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glViewport(0, 0, g_win_w, g_win_h);
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
                glVertex2f(420, g_win_h - 200); glVertex2f(10, g_win_h - 200);
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
                snprintf(dbg, sizeof(dbg), "Game: 960x544 -> Window: %dx%d", g_win_w, g_win_h);
                g_gui.draw_string(dbg, 20, g_win_h - 135, 1.2f, 140, 140, 160, 255);
                snprintf(dbg, sizeof(dbg), "Mouse: %d,%d  DT: %.4fs", mouse_x, mouse_y, dt_seconds);
                g_gui.draw_string(dbg, 20, g_win_h - 152, 1.2f, 140, 140, 160, 255);
                snprintf(dbg, sizeof(dbg), "F1:GUI  F3:Debug  F12:Full  F2:Camera");
                g_gui.draw_string(dbg, 20, g_win_h - 175, 1.1f, 100, 100, 120, 200);
                char cam_dbg[128];
                cam_debug_string(cam_dbg, sizeof(cam_dbg));
                g_gui.draw_string(cam_dbg, 20, g_win_h - 195, 1.1f,
                    g_cam_active ? 0 : 120,
                    g_cam_active ? 220 : 120,
                    g_cam_active ? 100 : 120, 255);
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
                glPopAttrib();
            }
            
            // Show "Press F1" hint for first 5 seconds
            if (g_display_active && !gui_visible && (SDL_GetTicks() - hint_start_time) < 5000) {
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glViewport(0, 0, g_win_w, g_win_h);
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
                glVertex2f(cx - 260, cy - 15); glVertex2f(cx + 260, cy - 15);
                glVertex2f(cx + 260, cy + 15); glVertex2f(cx - 260, cy + 15);
                glEnd();
                g_gui.draw_string("Press F1 for GUI | F3 for Debug | F12 Fullscreen", cx - 250, cy - 7, 1.2f, 0, 200, 255, 255);
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
                glPopAttrib();
            }
            
            extern int g_gl_diag_frame;
            g_gl_diag_frame++;
            
            // Present the frame
            if (g_display_active && g_display_ptr) {
                
                g_display_ptr->swap();
                
                // Poll and process SDL window and input events
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    switch (event.type) {
                        case SDL_QUIT:
                            running = false;
                            break;
                        case SDL_WINDOWEVENT:
                            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                                running = false;
                            }
                            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                                g_win_w = event.window.data1;
                                g_win_h = event.window.data2;
                                std::cout << "[Display] Resized to " << g_win_w << "x" << g_win_h << std::endl;
                            }
                            break;
                        case SDL_MOUSEBUTTONDOWN:
                            if (event.button.button == SDL_BUTTON_LEFT) {
                                mouse_pressed = true;
                                mouse_x = event.button.x;
                                mouse_y = g_win_h - event.button.y;
                                if (gui_visible && g_gui.handle_click(mouse_x, mouse_y)) {
                                    click_swallowed_by_gui = true;
                                } else {
                                    click_swallowed_by_gui = false;
                                    // Scale mouse from window to 960x544 game coords
                                    float x = event.button.x * 960.0f / (float)g_win_w;
                                    float y = 544.0f - (event.button.y * 544.0f / (float)g_win_h);
                                    last_mouse_x = x;
                                    last_mouse_y = y;
                                    call_handle_touch_event(handleTouchEvent, env_ptr, 0, 1, 1, accumulated_time, x, y, x, y, 1);
                                }
                            }
                            break;
                        case SDL_MOUSEMOTION:
                            mouse_x = event.motion.x;
                            mouse_y = g_win_h - event.motion.y;
                            if (mouse_pressed && !click_swallowed_by_gui) {
                                float x = event.motion.x * 960.0f / (float)g_win_w;
                                float y = 544.0f - (event.motion.y * 544.0f / (float)g_win_h);
                                call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, 1, accumulated_time, x, y, last_mouse_x, last_mouse_y, 0);
                                last_mouse_x = x;
                                last_mouse_y = y;
                            }
                            break;
                        case SDL_MOUSEBUTTONUP:
                            if (event.button.button == SDL_BUTTON_LEFT) {
                                mouse_pressed = false;
                                mouse_x = event.button.x;
                                mouse_y = g_win_h - event.button.y;
                                if (!click_swallowed_by_gui) {
                                    float x = event.button.x * 960.0f / (float)g_win_w;
                                    float y = 544.0f - (event.button.y * 544.0f / (float)g_win_h);
                                    call_handle_touch_event(handleTouchEvent, env_ptr, 0, 2, 1, accumulated_time, x, y, last_mouse_x, last_mouse_y, 0);
                                    last_mouse_x = -1.0f;
                                    last_mouse_y = -1.0f;
                                }
                                click_swallowed_by_gui = false;
                            }
                            break;
                        case SDL_KEYDOWN:
                            if (event.key.keysym.sym == SDLK_F1 && !event.key.repeat) {
                                gui_visible = !gui_visible;
                                std::cout << "[GUI] " << (gui_visible ? "ON" : "OFF") << std::endl;
                                break;
                            }
                            if (event.key.keysym.sym == SDLK_F2 && !event.key.repeat) {
                                cam_set_active(!g_cam_active);
                                break;
                            }
                            if (event.key.keysym.sym == SDLK_F3 && !event.key.repeat) {
                                debug_visible = !debug_visible;
                                std::cout << "[Debug] " << (debug_visible ? "ON" : "OFF") << std::endl;
                                break;
                            }
                            if (event.key.keysym.sym == SDLK_F12 && !event.key.repeat) {
                                Uint32 flags = SDL_GetWindowFlags(g_display_ptr->get_window());
                                if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                                    SDL_SetWindowFullscreen(g_display_ptr->get_window(), 0);
                                    g_win_w = 1920;
                                    g_win_h = 1080;
                                    std::cout << "[Display] Windowed 1920x1080" << std::endl;
                                } else {
                                    SDL_SetWindowFullscreen(g_display_ptr->get_window(), SDL_WINDOW_FULLSCREEN_DESKTOP);
                                    // Get actual fullscreen resolution
                                    int fw, fh;
                                    SDL_GetWindowSize(g_display_ptr->get_window(), &fw, &fh);
                                    g_win_w = fw;
                                    g_win_h = fh;
                                    std::cout << "[Display] Fullscreen " << fw << "x" << fh << std::endl;
                                }
                                break;
                            }
                            // fall through
                        case SDL_KEYUP: {
                            bool is_down = (event.type == SDL_KEYDOWN);
                            switch (event.key.keysym.sym) {
                                case SDLK_LEFT:
                                case SDLK_a:
                                    if (key_left != is_down) {
                                        key_left = is_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, is_down ? 1 : 2, 6, accumulated_time, 60.0f, 94.0f, 60.0f, 94.0f, is_down ? 1 : 0);
                                    }
                                    break;
                                case SDLK_RIGHT:
                                case SDLK_d:
                                    if (key_right != is_down) {
                                        key_right = is_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, is_down ? 1 : 2, 7, accumulated_time, 155.0f, 94.0f, 155.0f, 94.0f, is_down ? 1 : 0);
                                    }
                                    break;
                                case SDLK_SPACE:
                                case SDLK_UP:
                                case SDLK_w:
                                    if (key_jump != is_down) {
                                        key_jump = is_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, is_down ? 1 : 2, 5, accumulated_time, 900.0f, 94.0f, 900.0f, 94.0f, is_down ? 1 : 0);
                                    }
                                    break;
                                case SDLK_j:
                                case SDLK_z:
                                    if (key_attack != is_down) {
                                        key_attack = is_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, is_down ? 1 : 2, 8, accumulated_time, 790.0f, 94.0f, 790.0f, 94.0f, is_down ? 1 : 0);
                                    }
                                    break;
                                case SDLK_k:
                                case SDLK_x:
                                    if (key_magic != is_down) {
                                        key_magic = is_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, is_down ? 1 : 2, 10, accumulated_time, 900.0f, 184.0f, 900.0f, 184.0f, is_down ? 1 : 0);
                                    }
                                    break;
                                case SDLK_i:
                                    if (key_use_item != is_down) {
                                        key_use_item = is_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, is_down ? 1 : 2, 12, accumulated_time, 425.0f, 54.0f, 425.0f, 54.0f, is_down ? 1 : 0);
                                    }
                                    break;
                                case SDLK_ESCAPE:
                                    if (key_menu != is_down) {
                                        key_menu = is_down;
                                        call_handle_touch_event(handleTouchEvent, env_ptr, 0, is_down ? 1 : 2, 9, accumulated_time, 48.0f, 500.0f, 48.0f, 500.0f, is_down ? 1 : 0);
                                    }
                                    break;

                                // ---- Camera override keys (Numpad) ----
                                case SDLK_KP_4:    cam_key_left  = is_down; break;
                                case SDLK_KP_6:    cam_key_right = is_down; break;
                                case SDLK_KP_8:    cam_key_fwd   = is_down; break;
                                case SDLK_KP_2:    cam_key_back  = is_down; break;
                                case SDLK_KP_7:
                                case SDLK_PAGEUP:   cam_key_up   = is_down; break;
                                case SDLK_KP_1:
                                case SDLK_PAGEDOWN: cam_key_down = is_down; break;
                                case SDLK_KP_PLUS:  cam_key_zin  = is_down; break;
                                case SDLK_KP_MINUS: cam_key_zout = is_down; break;
                                case SDLK_KP_5:  // Reset camera position
                                case SDLK_HOME:
                                    if (is_down) cam_reset();
                                    break;
                            }
                            break;
                        }
                        // --- Multi-touch support (touchscreen laptops, up to 10 fingers) ---
                        case SDL_FINGERDOWN: {
                            // SDL finger coords are normalized 0.0-1.0, scale to game 960x544
                            float x = event.tfinger.x * 960.0f;
                            float y = (1.0f - event.tfinger.y) * 544.0f;  // flip Y
                            int finger_id = (int)(event.tfinger.fingerId % 10) + 20;  // IDs 20-29 for touch
                            call_handle_touch_event(handleTouchEvent, env_ptr, 0, 1, finger_id, accumulated_time, x, y, x, y, 1);
                            break;
                        }
                        case SDL_FINGERUP: {
                            float x = event.tfinger.x * 960.0f;
                            float y = (1.0f - event.tfinger.y) * 544.0f;
                            int finger_id = (int)(event.tfinger.fingerId % 10) + 20;
                            call_handle_touch_event(handleTouchEvent, env_ptr, 0, 2, finger_id, accumulated_time, x, y, x, y, 0);
                            break;
                        }
                        case SDL_FINGERMOTION: {
                            float x = event.tfinger.x * 960.0f;
                            float y = (1.0f - event.tfinger.y) * 544.0f;
                            float old_x = (event.tfinger.x - event.tfinger.dx) * 960.0f;
                            float old_y = (1.0f - (event.tfinger.y - event.tfinger.dy)) * 544.0f;
                            int finger_id = (int)(event.tfinger.fingerId % 10) + 20;
                            call_handle_touch_event(handleTouchEvent, env_ptr, 0, 4, finger_id, accumulated_time, x, y, old_x, old_y, 0);
                            break;
                        }
                    }
                }
            }
            
            // FPS counter
            fps_frame_count++;
            Uint32 fps_now = SDL_GetTicks();
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

int main(int argc, char* argv[]) {
    // Check for --headless flag
    bool headless = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) headless = true;
    }
    
    // Set up user-writable directory paths (XDG Base Directory standard)
    std::string home_dir = getenv("HOME") ? getenv("HOME") : ".";
    std::string xdg_data = getenv("XDG_DATA_HOME") ? getenv("XDG_DATA_HOME") : (home_dir + "/.local/share");
    std::string base_dir = xdg_data + "/swordigo-desktop";
    mkdir(xdg_data.c_str(), 0755);
    mkdir(base_dir.c_str(), 0755);
    
    g_save_dir = base_dir + "/save";
    g_cache_dir = base_dir + "/cache";
    mkdir(g_save_dir.c_str(), 0755);
    mkdir(g_cache_dir.c_str(), 0755);
    std::cout << "[Main] Save directory: " << g_save_dir << std::endl;
    std::cout << "[Main] Cache directory: " << g_cache_dir << std::endl;
    
    Display display;
    
    if (!headless) {
        if (display.init(1920, 1080, "Swordigo Desktop - Remastered")) {
            g_display_active = true;
            g_display_ptr = &display;
            std::cout << "[Main] Display initialized - 1920x1080 HD" << std::endl;
        } else {
            std::cerr << "[Main] Display init failed, falling back to headless" << std::endl;
        }
    } else {
        std::cout << "[Main] Running in headless mode" << std::endl;
    }
    
    init_all();
    load_and_boot();
    
    std::cout << "[Main] Swordigo Desktop Engine — session complete." << std::endl;
    
    return 0;
}

