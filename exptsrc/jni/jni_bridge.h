#ifndef JNI_BRIDGE_H
#define JNI_BRIDGE_H

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <functional>

// Magic base for host-native function bridges
#define BRIDGE_BASE 0xFF000000

typedef void (*BridgeHandler)(void* emu);

struct BridgeFunction {
    std::string name;
    uint32_t address;
    BridgeHandler handler;
};

// --- Frame Statistics ---
struct FrameStats {
    uint32_t draw_calls = 0;        // glDrawArrays + glDrawElements
    uint32_t texture_binds = 0;     // glBindTexture
    uint32_t tex_uploads = 0;       // glTexImage2D + glCompressedTexImage2D
    uint32_t matrix_ops = 0;        // glLoadMatrixf, glPushMatrix, etc.
    uint32_t state_changes = 0;     // glEnable, glDisable, glBlendFunc, etc.
    uint32_t asset_opens = 0;       // AAssetManager_open
    uint32_t clear_calls = 0;       // glClear
    uint32_t viewport_calls = 0;    // glViewport
    
    // Mission 12: Geometry tracking
    uint32_t vertices_submitted = 0;    // sum of count args to glDrawArrays/Elements
    uint32_t vertex_pointer_calls = 0;  // glVertexPointer
    uint32_t texcoord_pointer_calls = 0;// glTexCoordPointer
    uint32_t color_pointer_calls = 0;   // glColorPointer
    
    // Mission 13: Key asset detection
    bool title_texture_loaded = false;     // swordigo_title*.pvr/png
    bool menu_scene_loaded = false;        // menu.scene
    bool menu_back_loaded = false;         // menu_back.POD
    bool common_atlas_loaded = false;      // game_common_atlas*.pvr
    
    // Mission 14: Last bound texture ID
    uint32_t last_bound_texture = 0;
    uint32_t unique_textures_bound = 0;
    
    void reset() {
        draw_calls = texture_binds = tex_uploads = matrix_ops = 0;
        state_changes = asset_opens = clear_calls = viewport_calls = 0;
        vertices_submitted = vertex_pointer_calls = texcoord_pointer_calls = color_pointer_calls = 0;
        last_bound_texture = 0;
        unique_textures_bound = 0;
        // Don't reset asset proof flags — they're cumulative across boot
    }
};

extern FrameStats g_frame_stats;


class JniBridge {
public:
    JniBridge();
    
    uint32_t get_address(const std::string& name);
    std::string get_name(uint32_t address);
    // Returns bridge address for a registered handler, or 0 if unknown.
    uint32_t lookup_proc_address(const std::string& name);
    
    void register_handler(const std::string& name, BridgeHandler handler);
    void call_handler(uint32_t address, void* emu);

    // Initialize standard GLES/AL/Bionic bridges
    void init_standard_bridges();

private:
    std::map<std::string, uint32_t> name_to_addr;
    std::map<uint32_t, BridgeFunction> addr_to_func;
    uint32_t next_addr;
};

#endif

