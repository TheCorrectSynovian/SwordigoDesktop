#include <iostream>
#include <vector>
#include <stdint.h>
#include "loader/elf_loader.h"
#include "jni/jni_layer.h"
#include "jni/jni_bridge.h"
#include "platform/emulator.h"
#include "platform/display.h"
#include "android/asset_manager.h"
#include <cstring>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

extern bool g_display_active;

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

void init_all() {
    g_guest_memory = new uint8_t[GUEST_MEM_SIZE];
    g_loader = new ElfLoader(g_guest_memory, GUEST_MEM_SIZE);
    g_bridge.init_standard_bridges();
    asset_manager_init("assets/resources");
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
    *(uint32_t*)(memory + vtable_ptr + 0x2CC) = g_bridge.get_address("NewIntArray");
    *(uint32_t*)(memory + vtable_ptr + 0x2EC) = g_bridge.get_address("GetIntArrayElements");
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
    std::string so_path = "libswordigo.so";
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
    strcpy((char*)(g_guest_memory + files_dir), "./save");
    path_ptr += 100;
    
    uint32_t cache_dir = path_ptr;
    strcpy((char*)(g_guest_memory + cache_dir), "./cache");
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
        g_emulator->call(initMusicPlayer, {env_ptr, 0});
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
        g_emulator->call(setApplicationViewSize, {env_ptr, 0, 800, 480, 1}); // 800x480, is_pad=1
    }
    if (applicationDidBecomeActive) {
        std::cout << "[Boot] Calling applicationDidBecomeActive" << std::endl;
        g_emulator->call(applicationDidBecomeActive, {env_ptr, 0});
    }

    // 5. Game Loop — 1000-frame stability test
    uint32_t updateApp = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_updateApplication");
    uint32_t drawApp = g_loader->get_symbol_vaddr(&g_main_mod, "Java_com_touchfoo_swordigo_Native_drawApplication");

    if (updateApp && drawApp) {
        // IEEE 754 for 1/60 (~0.01667f) = 0x3c888889
        uint32_t dt_hex = 0x3c888889;
        
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

        // Suppress verbose per-call logging during game loop
        g_emulator->quiet_mode = true;

        // Get the display pointer from main (passed via extern)
        extern Display* g_display_ptr;

        bool running = true;
        while (running) {
            g_frame_stats.reset();
            
            g_emulator->call(updateApp, {env_ptr, 0, dt_hex});
            g_emulator->call(drawApp, {env_ptr, 0});
            
            // --- Diagnostic Triangle Test (Mission 10) ---
            if (g_display_active && g_display_ptr) {
                // To avoid interfering with engine state, we do this after drawApp
                // glClear(GL_DEPTH_BUFFER_BIT); // Optional

                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();

                glDisable(GL_TEXTURE_2D);
                glDisable(GL_LIGHTING);
                glDisable(GL_CULL_FACE);
                glDisable(GL_DEPTH_TEST);

                glBegin(GL_TRIANGLES);
                glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(-0.5f, -0.5f);
                glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(0.5f, -0.5f);
                glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(0.0f, 0.5f);
                glEnd();

                glMatrixMode(GL_PROJECTION); glPopMatrix();
                glMatrixMode(GL_MODELVIEW); glPopMatrix();

                extern int g_gl_diag_frame;
                g_display_ptr->swap();
                g_gl_diag_frame++;
                g_display_ptr->poll_events();
                if (g_display_ptr->should_close()) running = false;
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
    
    Display display;
    
    if (!headless) {
        if (display.init(800, 480, "Swordigo Desktop")) {
            g_display_active = true;
            g_display_ptr = &display;
            
            // Enable diagnostics (Mission 10)
            extern bool g_gl_force_white;
            extern bool g_gl_force_identity;
            g_gl_force_white = true;
            g_gl_force_identity = true;

            std::cout << "[Main] Display initialized — visual mode (Diagnostics ENABLED)" << std::endl;
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

