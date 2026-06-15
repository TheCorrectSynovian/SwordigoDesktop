#include "jni_bridge.h"
#include "platform/emulator.h"
#include "android/asset_manager.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

// When true, GL bridge functions call real OpenGL instead of no-ops
bool g_display_active = false;


// --- Frame Statistics ---
FrameStats g_frame_stats;

// --- Handle Management (Restored from Agent 1) ---
static std::unordered_map<uint32_t, void*> g_handle_to_ptr;
static std::unordered_map<void*, uint32_t> g_ptr_to_handle;
static uint32_t g_next_handle = 0x88880001;
static std::mutex g_handles_mutex;

uint32_t register_pointer(void* ptr) {
    if (!ptr) return 0;
    std::lock_guard<std::mutex> lock(g_handles_mutex);
    if (g_ptr_to_handle.count(ptr)) return g_ptr_to_handle[ptr];
    uint32_t handle = g_next_handle++;
    g_handle_to_ptr[handle] = ptr;
    g_ptr_to_handle[ptr] = handle;
    return handle;
}

void* get_pointer(uint32_t handle) {
    if (handle == 0) return nullptr;
    std::lock_guard<std::mutex> lock(g_handles_mutex);
    if (g_handle_to_ptr.count(handle)) return g_handle_to_ptr[handle];
    return (void*)(uintptr_t)handle; 
}

void release_pointer(void* ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(g_handles_mutex);
    if (g_ptr_to_handle.count(ptr)) {
        uint32_t handle = g_ptr_to_handle[ptr];
        g_ptr_to_handle.erase(ptr);
        g_handle_to_ptr.erase(handle);
    }
}

// --- Soft-Float Register Conversion Helpers ---
static float uint_to_float(uint32_t u) {
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

static uint32_t float_to_uint(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return u;
}

// --- JniBridge Methods ---
JniBridge::JniBridge() : next_addr(BRIDGE_BASE) {}

uint32_t JniBridge::get_address(const std::string& name) {
    if (name_to_addr.count(name)) return name_to_addr[name];
    
    uint32_t addr = next_addr;
    next_addr += 4;
    
    name_to_addr[name] = addr;
    addr_to_func[addr] = {name, addr, nullptr};
    
    return addr;
}

std::string JniBridge::get_name(uint32_t address) {
    if (addr_to_func.count(address)) return addr_to_func[address].name;
    return "UnknownBridge";
}

uint32_t JniBridge::lookup_proc_address(const std::string& name) {
    auto it = name_to_addr.find(name);
    if (it == name_to_addr.end()) return 0;
    uint32_t addr = it->second;
    auto fit = addr_to_func.find(addr);
    if (fit == addr_to_func.end() || !fit->second.handler) return 0;
    return addr;
}

void JniBridge::register_handler(const std::string& name, BridgeHandler handler) {
    uint32_t addr = get_address(name);
    addr_to_func[addr].handler = handler;
}

void JniBridge::call_handler(uint32_t address, void* emu_ptr) {
    if (addr_to_func.count(address)) {
        BridgeFunction& func = addr_to_func[address];
        Emulator* emu = (Emulator*)emu_ptr;
        // Only log when not in quiet mode, and skip trivial calls
        if (!emu->quiet_mode) {
            static const std::unordered_map<std::string, bool> quiet_funcs = {
                {"memcpy",1},{"memset",1},{"memmove",1},{"memcmp",1},{"memchr",1},
                {"strlen",1},{"malloc",1},{"calloc",1},{"realloc",1},{"free",1},
                {"__aeabi_memclr",1},{"__aeabi_memclr4",1},{"__aeabi_memclr8",1},
                {"__aeabi_memset",1},{"__aeabi_memset4",1},{"__aeabi_memset8",1},
                {"__aeabi_memcpy",1},{"__aeabi_memcpy4",1},{"__aeabi_memcpy8",1},
                {"__aeabi_memmove",1},{"__aeabi_memmove4",1},{"__aeabi_memmove8",1},
                {"__aeabi_uidiv",1},{"__aeabi_idiv",1},{"__aeabi_uidivmod",1},
                {"__aeabi_idivmod",1},{"__aeabi_ldivmod",1},{"__aeabi_uldivmod",1},
                {"strchr",1},{"strrchr",1},{"strcpy",1},{"strncpy",1},
                {"strcmp",1},{"strncmp",1},{"strcat",1},{"strstr",1},
                {"cosf",1},{"sinf",1},{"roundf",1},{"floorf",1},{"ceilf",1},
                {"sqrtf",1},{"atan2f",1},{"powf",1},{"sincosf",1},
                {"strtol",1},{"strtoul",1},{"atoi",1},{"atof",1},
                {"pthread_mutex_init",1},{"pthread_mutex_lock",1},{"pthread_mutex_unlock",1},
                {"pthread_mutex_destroy",1},{"pthread_cond_init",1},{"pthread_cond_signal",1},
                {"pthread_cond_wait",1},{"pthread_cond_destroy",1},{"pthread_cond_broadcast",1},
            };
            if (!quiet_funcs.count(func.name)) {
                std::cout << "[Bridge] Call: " << func.name << std::endl;
            }
        }
        if (func.handler) {
            func.handler(emu_ptr);
        } else {
            static std::unordered_set<std::string> warned_funcs;
            if (!warned_funcs.count(func.name)) {
                std::cout << "[WARNING] Call to UNIMPLEMENTED bridge function: " << func.name << std::endl;
                warned_funcs.insert(func.name);
            }
        }
    }
}



// --- Memory Allocator Bridges ---
static uint32_t g_guest_heap_ptr = 0x20000000; // Start heap at 512MB
static std::unordered_map<uint32_t, uint32_t> g_guest_allocs;

void bridge_malloc(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t size = emu->get_reg(0);
    uint32_t addr = g_guest_heap_ptr;
    g_guest_heap_ptr += (size + 7) & ~7;
    g_guest_allocs[addr] = size;
    emu->set_reg(0, addr);
}

void bridge_calloc(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t num = emu->get_reg(0);
    uint32_t size = emu->get_reg(1);
    uint32_t total = num * size;
    uint32_t addr = g_guest_heap_ptr;
    g_guest_heap_ptr += (total + 7) & ~7;
    g_guest_allocs[addr] = total;
    std::memset(emu->get_memory_base() + addr, 0, total);
    emu->set_reg(0, addr);
}

void bridge_realloc(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t ptr = emu->get_reg(0);
    uint32_t size = emu->get_reg(1);
    if (ptr == 0) {
        uint32_t addr = g_guest_heap_ptr;
        g_guest_heap_ptr += (size + 7) & ~7;
        g_guest_allocs[addr] = size;
        emu->set_reg(0, addr);
        return;
    }
    if (size == 0) {
        emu->set_reg(0, 0);
        return;
    }
    uint32_t old_size = 0;
    if (g_guest_allocs.count(ptr)) {
        old_size = g_guest_allocs[ptr];
    }
    uint32_t addr = g_guest_heap_ptr;
    g_guest_heap_ptr += (size + 7) & ~7;
    g_guest_allocs[addr] = size;
    uint32_t copy_size = (old_size < size) ? old_size : size;
    if (copy_size > 0) {
        std::memcpy(emu->get_memory_base() + addr, emu->get_memory_base() + ptr, copy_size);
    }
    emu->set_reg(0, addr);
}

void bridge_free(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t ptr = emu->get_reg(0);
    if (ptr) {
        g_guest_allocs.erase(ptr);
    }
}

// --- Standard C Memory & String Bridges ---
void bridge_memcpy(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t dest = emu->get_reg(0);
    uint32_t src = emu->get_reg(1);
    uint32_t n = emu->get_reg(2);
    if (n > 0) {
        std::memcpy(emu->get_memory_base() + dest, emu->get_memory_base() + src, n);
    }
    emu->set_reg(0, dest);
}

void bridge_memset(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t dest = emu->get_reg(0);
    uint32_t c = emu->get_reg(1);
    uint32_t n = emu->get_reg(2);
    if (n > 0) {
        std::memset(emu->get_memory_base() + dest, c, n);
    }
    emu->set_reg(0, dest);
}

void bridge_memmove(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t dest = emu->get_reg(0);
    uint32_t src = emu->get_reg(1);
    uint32_t n = emu->get_reg(2);
    if (n > 0) {
        std::memmove(emu->get_memory_base() + dest, emu->get_memory_base() + src, n);
    }
    emu->set_reg(0, dest);
}

void bridge_strlen(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t str = emu->get_reg(0);
    const char* s = (const char*)(emu->get_memory_base() + str);
    emu->set_reg(0, std::strlen(s));
}

void bridge_memcmp(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t str1 = emu->get_reg(0);
    uint32_t str2 = emu->get_reg(1);
    uint32_t n = emu->get_reg(2);
    int res = 0;
    if (n > 0) {
        res = std::memcmp(emu->get_memory_base() + str1, emu->get_memory_base() + str2, n);
    }
    emu->set_reg(0, res);
}

// --- EABI Memory Helpers ---
void bridge_aeabi_memclr(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t dest = emu->get_reg(0);
    uint32_t n = emu->get_reg(1);
    if (n > 0) {
        std::memset(emu->get_memory_base() + dest, 0, n);
    }
}

void bridge_aeabi_memset(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t dest = emu->get_reg(0);
    uint32_t n = emu->get_reg(1);
    uint32_t c = emu->get_reg(2);
    if (n > 0) {
        std::memset(emu->get_memory_base() + dest, c, n);
    }
}

void bridge_aeabi_memcpy(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t dest = emu->get_reg(0);
    uint32_t src = emu->get_reg(1);
    uint32_t n = emu->get_reg(2);
    if (n > 0) {
        std::memcpy(emu->get_memory_base() + dest, emu->get_memory_base() + src, n);
    }
}

void bridge_aeabi_memmove(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t dest = emu->get_reg(0);
    uint32_t src = emu->get_reg(1);
    uint32_t n = emu->get_reg(2);
    if (n > 0) {
        std::memmove(emu->get_memory_base() + dest, emu->get_memory_base() + src, n);
    }
}

// --- EABI Division Helpers ---
void bridge_aeabi_uidiv(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t num = emu->get_reg(0);
    uint32_t den = emu->get_reg(1);
    emu->set_reg(0, (den == 0) ? 0 : (num / den));
}

void bridge_aeabi_idiv(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    int32_t num = (int32_t)emu->get_reg(0);
    int32_t den = (int32_t)emu->get_reg(1);
    emu->set_reg(0, (den == 0) ? 0 : (num / den));
}

void bridge_aeabi_uidivmod(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t num = emu->get_reg(0);
    uint32_t den = emu->get_reg(1);
    if (den == 0) {
        emu->set_reg(0, 0);
        emu->set_reg(1, 0);
    } else {
        emu->set_reg(0, num / den);
        emu->set_reg(1, num % den);
    }
}

void bridge_aeabi_idivmod(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    int32_t num = (int32_t)emu->get_reg(0);
    int32_t den = (int32_t)emu->get_reg(1);
    if (den == 0) {
        emu->set_reg(0, 0);
        emu->set_reg(1, 0);
    } else {
        emu->set_reg(0, num / den);
        emu->set_reg(1, num % den);
    }
}

void bridge_aeabi_ldivmod(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint64_t num_low = emu->get_reg(0);
    uint64_t num_high = emu->get_reg(1);
    uint64_t den_low = emu->get_reg(2);
    uint64_t den_high = emu->get_reg(3);
    int64_t num = (int64_t)((num_high << 32) | num_low);
    int64_t den = (int64_t)((den_high << 32) | den_low);
    if (den == 0) {
        emu->set_reg(0, 0); emu->set_reg(1, 0);
        emu->set_reg(2, 0); emu->set_reg(3, 0);
    } else {
        int64_t quot = num / den;
        int64_t rem = num % den;
        emu->set_reg(0, (uint32_t)(quot & 0xFFFFFFFF));
        emu->set_reg(1, (uint32_t)((quot >> 32) & 0xFFFFFFFF));
        emu->set_reg(2, (uint32_t)(rem & 0xFFFFFFFF));
        emu->set_reg(3, (uint32_t)((rem >> 32) & 0xFFFFFFFF));
    }
}

void bridge_aeabi_uldivmod(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint64_t num_low = emu->get_reg(0);
    uint64_t num_high = emu->get_reg(1);
    uint64_t den_low = emu->get_reg(2);
    uint64_t den_high = emu->get_reg(3);
    uint64_t num = ((num_high << 32) | num_low);
    uint64_t den = ((den_high << 32) | den_low);
    if (den == 0) {
        emu->set_reg(0, 0); emu->set_reg(1, 0);
        emu->set_reg(2, 0); emu->set_reg(3, 0);
    } else {
        uint64_t quot = num / den;
        uint64_t rem = num % den;
        emu->set_reg(0, (uint32_t)(quot & 0xFFFFFFFF));
        emu->set_reg(1, (uint32_t)((quot >> 32) & 0xFFFFFFFF));
        emu->set_reg(2, (uint32_t)(rem & 0xFFFFFFFF));
        emu->set_reg(3, (uint32_t)((rem >> 32) & 0xFFFFFFFF));
    }
}

// --- Soft-Float Math Bridges ---
void bridge_cosf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::cos(x)));
}

void bridge_sinf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::sin(x)));
}

void bridge_tanf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::tan(x)));
}

void bridge_roundf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::round(x)));
}

void bridge_floorf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::floor(x)));
}

void bridge_ceilf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::ceil(x)));
}

void bridge_sqrtf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::sqrt(x)));
}


void bridge_acosf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::acos(x)));
}

void bridge_asinf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::asin(x)));
}

void bridge_atanf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::atan(x)));
}

void bridge_fmodf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    float y = uint_to_float(emu->get_reg(1));
    emu->set_reg(0, float_to_uint(std::fmod(x, y)));
}

void bridge_expf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::exp(x)));
}

void bridge_logf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::log(x)));
}

void bridge_log10f(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    emu->set_reg(0, float_to_uint(std::log10(x)));
}

void bridge_atan2f(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float y = uint_to_float(emu->get_reg(0));
    float x = uint_to_float(emu->get_reg(1));
    emu->set_reg(0, float_to_uint(std::atan2(y, x)));
}

void bridge_powf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float base = uint_to_float(emu->get_reg(0));
    float exp = uint_to_float(emu->get_reg(1));
    emu->set_reg(0, float_to_uint(std::pow(base, exp)));
}

void bridge_sincosf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    float x = uint_to_float(emu->get_reg(0));
    uint32_t sin_ptr = emu->get_reg(1);
    uint32_t cos_ptr = emu->get_reg(2);
    float s = std::sin(x);
    float c = std::cos(x);
    uint8_t* memory = emu->get_memory_base();
    *(uint32_t*)(memory + sin_ptr) = float_to_uint(s);
    *(uint32_t*)(memory + cos_ptr) = float_to_uint(c);
}

// --- JNI Standard Bridges ---
void bridge_FindClass(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t name_ptr = emu->get_reg(1);
    const char* name = (const char*)(memory + name_ptr);
    std::cout << "[JNI] FindClass: " << name << std::endl;
    emu->set_reg(0, 0x12340001);
}

void bridge_GetMethodID(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t name_ptr = emu->get_reg(2);
    const char* name = (const char*)(memory + name_ptr);
    // Return unique IDs per method name (matching Vita approach)
    uint32_t id;
    if (strcmp(name, "getPlatformConsentState") == 0) id = 0x13180001;
    else if (strcmp(name, "isAgeKnown") == 0) id = 0x13170001;
    else if (strcmp(name, "loadFile") == 0) id = 0x13190001;
    else if (strcmp(name, "play") == 0) id = 0x13200001;
    else if (strcmp(name, "pause") == 0) id = 0x13210001;
    else if (strcmp(name, "stop") == 0) id = 0x13220001;
    else if (strcmp(name, "setLooping") == 0) id = 0x13230001;
    else if (strcmp(name, "setVolume") == 0) id = 0x13240001;
    else if (strcmp(name, "<init>") == 0) id = 0x13000001;
    else id = 0x56780001;
    if (!emu->quiet_mode || strcmp(name, "getPlatformConsentState") == 0 || strcmp(name, "isAgeKnown") == 0) {
        std::cout << "[JNI] GetMethodID: " << name << " -> 0x" << std::hex << id << std::dec << std::endl;
    }
    emu->set_reg(0, id);
}

void bridge_RegisterNatives(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_NewGlobalRef(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t obj = emu->get_reg(1);
    emu->set_reg(0, obj);
}

void bridge_NewStringUTF(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0x99990001);
}

void bridge_GetStringUTFChars(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t jstr = emu->get_reg(1);
    if (jstr >= 0x20000 && jstr < 0x21000) {
        emu->set_reg(0, jstr);
    } else {
        uint32_t addr = 0x30000;
        strcpy((char*)(emu->get_memory_base() + addr), "dummy_jni_string");
        emu->set_reg(0, addr);
    }
}

void bridge_AAssetManager_fromJava(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    void* mgr = AAssetManager_fromJava(NULL, NULL);
    emu->set_reg(0, register_pointer(mgr));
}

void bridge_AAssetManager_open(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    AAssetManager* mgr = (AAssetManager*)get_pointer(emu->get_reg(0));
    uint32_t filename_ptr = emu->get_reg(1);
    const char* filename = (const char*)(memory + filename_ptr);
    g_frame_stats.asset_opens++;
    

    // Mission 13: Asset proof detection
    std::string fname(filename);
    if (fname.find("swordigo_title") != std::string::npos && !g_frame_stats.title_texture_loaded) {
        g_frame_stats.title_texture_loaded = true;
        std::cout << "[ASSET PROOF] ✓ Title texture loaded: " << filename << std::endl;
    }
    if (fname == "resources/menu.scene" && !g_frame_stats.menu_scene_loaded) {
        g_frame_stats.menu_scene_loaded = true;
        std::cout << "[ASSET PROOF] ✓ Menu scene loaded: " << filename << std::endl;
    }
    if (fname.find("menu_back") != std::string::npos && !g_frame_stats.menu_back_loaded) {
        g_frame_stats.menu_back_loaded = true;
        std::cout << "[ASSET PROOF] ✓ Menu background loaded: " << filename << std::endl;
    }
    if (fname.find("game_common_atlas") != std::string::npos && !g_frame_stats.common_atlas_loaded) {
        g_frame_stats.common_atlas_loaded = true;
        std::cout << "[ASSET PROOF] ✓ Common atlas loaded: " << filename << std::endl;
    }
    
    void* asset = AAssetManager_open(mgr, filename, emu->get_reg(2));
    emu->set_reg(0, register_pointer(asset));
}



void bridge_AAsset_read(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    AAsset* asset = (AAsset*)get_pointer(emu->get_reg(0));
    void* buf = (void*)(memory + emu->get_reg(1));
    emu->set_reg(0, AAsset_read(asset, buf, emu->get_reg(2)));
}

void bridge_AAsset_close(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    AAsset* asset = (AAsset*)get_pointer(emu->get_reg(0));
    AAsset_close(asset);
    release_pointer(asset);
}

void bridge_AAsset_getLength(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    AAsset* asset = (AAsset*)get_pointer(emu->get_reg(0));
    emu->set_reg(0, (uint32_t)AAsset_getLength(asset));
}

void bridge_AAsset_openFileDescriptor(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    AAsset* asset = (AAsset*)get_pointer(emu->get_reg(0));
    off_t* outStart = (off_t*)(memory + emu->get_reg(1));
    off_t* outLength = (off_t*)(memory + emu->get_reg(2));
    emu->set_reg(0, AAsset_openFileDescriptor(asset, outStart, outLength));
}

// --- RESTORED JNI ENVIRONMENT SHIMS ---
void bridge_GetJavaVM(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t vm_ptr_ptr = emu->get_reg(1);
    *(uint32_t*)(memory + vm_ptr_ptr) = 0x11000; // Fake VM pointer
    emu->set_reg(0, 0); // JNI_OK
}

void bridge_GetEnv(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t env_ptr_ptr = emu->get_reg(2);
    *(uint32_t*)(memory + env_ptr_ptr) = 0x10000; // Fake Env pointer
    emu->set_reg(0, 0); // JNI_OK
}

void bridge_ThrowNew(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_PushLocalFrame(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_PopLocalFrame(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_DeleteLocalRef(void* emu_ptr) {
}

void bridge_DeleteGlobalRef(void* emu_ptr) {
}

void bridge_NewObjectV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0x43434343);
}

void bridge_CallObjectMethodV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t obj = emu->get_reg(1);
    uint32_t mid = emu->get_reg(2);

    // The engine calls getPlatformConsentState which returns a consent object.
    // If we return NULL, engine retries every frame. Return a valid fake object.
    if (mid == 0x13180001) {
        static int count = 0;
        count++;
        if (count <= 3) std::cout << "[JNI] CallObjectMethodV(getPlatformConsentState) -> returning fake consent obj" << std::endl;
        // Return a non-null fake consent object so engine doesn't retry
        emu->set_reg(0, 0x56565656);
    } else {
        emu->set_reg(0, 0x34343434);
    }
}

void bridge_CallBooleanMethodV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 1);
}

void bridge_CallIntMethodV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    // R0=env, R1=object, R2=methodID, R3=va_list
    uint32_t obj = emu->get_reg(1);
    uint32_t method_id = emu->get_reg(2);
    // ConsentStatus: 0=UNKNOWN, 1=NOT_REQUIRED, 2=OBTAINED, 3=REQUIRED
    // Return OBTAINED so engine proceeds
    if (method_id == 0x13180001) { // getPlatformConsentState -> reading consent status
        std::cout << "[JNI] CallIntMethodV -> returning CONSENT_STATUS_OBTAINED (2)" << std::endl;
        emu->set_reg(0, 2);
    } else {
        emu->set_reg(0, 0);
    }
}

void bridge_CallLongMethodV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
    emu->set_reg(1, 0);
}

void bridge_CallVoidMethodV(void* emu_ptr) {
}

void bridge_GetObjectClass(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0x44444444);
}

void bridge_GetFieldID(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_GetBooleanField(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 1);
}

void bridge_GetIntField(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_GetFloatField(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_CallStaticObjectMethodV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_CallStaticBooleanMethodV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t mid = emu->get_reg(2);
    // Vita project: IS_AGE_KNOWN and GET_PLATFORM_CONSENT_STATE both return 1
    if (mid == 0x13170001 || mid == 0x13180001) {
        emu->set_reg(0, 1);
    } else {
        emu->set_reg(0, 1);
    }
}

void bridge_CallStaticIntMethodV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_CallStaticLongMethodV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
    emu->set_reg(1, 0);
}

void bridge_CallStaticFloatMethodV(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_CallStaticVoidMethodV(void* emu_ptr) {
}

void bridge_GetStaticFieldID(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_GetStaticObjectField(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_GetStringUTFLength(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t jstr = emu->get_reg(1);
    const char* str = (const char*)(memory + jstr);
    emu->set_reg(0, std::strlen(str));
}

void bridge_ReleaseStringUTFChars(void* emu_ptr) {
}

void bridge_GetArrayLength(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_GetObjectArrayElement(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_NewIntArray(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_GetIntArrayElements(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}

void bridge_ReleaseIntArrayElements(void* emu_ptr) {
}

void bridge_SetIntArrayRegion(void* emu_ptr) {
}

void bridge_GetStringUTFRegion(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t jstr = emu->get_reg(1);
    uint32_t start = emu->get_reg(2);
    uint32_t len = emu->get_reg(3);
    uint32_t sp = emu->get_reg(13);
    uint32_t buf = *(uint32_t*)(memory + sp);
    const char* str = (const char*)(memory + jstr);
    std::memcpy(memory + buf, str + start, len);
    memory[buf + len] = '\0';
}

// --- Additional C String Bridges ---
void bridge_strchr(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t str = emu->get_reg(0);
    int c = (int)(emu->get_reg(1) & 0xFF);
    const char* s = (const char*)(memory + str);
    const char* result = std::strchr(s, c);
    if (result) {
        emu->set_reg(0, str + (uint32_t)(result - s));
    } else {
        emu->set_reg(0, 0);
    }
}

void bridge_strrchr(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t str = emu->get_reg(0);
    int c = (int)(emu->get_reg(1) & 0xFF);
    const char* s = (const char*)(memory + str);
    const char* result = std::strrchr(s, c);
    if (result) {
        emu->set_reg(0, str + (uint32_t)(result - s));
    } else {
        emu->set_reg(0, 0);
    }
}

void bridge_strcpy(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t dest = emu->get_reg(0);
    uint32_t src = emu->get_reg(1);
    std::strcpy((char*)(memory + dest), (const char*)(memory + src));
    emu->set_reg(0, dest);
}

void bridge_strncpy(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t dest = emu->get_reg(0);
    uint32_t src = emu->get_reg(1);
    uint32_t n = emu->get_reg(2);
    std::strncpy((char*)(memory + dest), (const char*)(memory + src), n);
    emu->set_reg(0, dest);
}

void bridge_strcmp(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t s1 = emu->get_reg(0);
    uint32_t s2 = emu->get_reg(1);
    int result = std::strcmp((const char*)(memory + s1), (const char*)(memory + s2));
    emu->set_reg(0, (uint32_t)result);
}

void bridge_strncmp(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t s1 = emu->get_reg(0);
    uint32_t s2 = emu->get_reg(1);
    uint32_t n = emu->get_reg(2);
    int result = std::strncmp((const char*)(memory + s1), (const char*)(memory + s2), n);
    emu->set_reg(0, (uint32_t)result);
}

void bridge_strcat(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t dest = emu->get_reg(0);
    uint32_t src = emu->get_reg(1);
    std::strcat((char*)(memory + dest), (const char*)(memory + src));
    emu->set_reg(0, dest);
}

void bridge_strstr(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t haystack = emu->get_reg(0);
    uint32_t needle = emu->get_reg(1);
    const char* h = (const char*)(memory + haystack);
    const char* n = (const char*)(memory + needle);
    const char* result = std::strstr(h, n);
    if (result) {
        emu->set_reg(0, haystack + (uint32_t)(result - h));
    } else {
        emu->set_reg(0, 0);
    }
}

void bridge_memchr_impl(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t ptr = emu->get_reg(0);
    int c = (int)(emu->get_reg(1) & 0xFF);
    uint32_t n = emu->get_reg(2);
    const void* result = std::memchr(memory + ptr, c, n);
    if (result) {
        emu->set_reg(0, ptr + (uint32_t)((const uint8_t*)result - (memory + ptr)));
    } else {
        emu->set_reg(0, 0);
    }
}

void bridge_strtol(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t str = emu->get_reg(0);
    uint32_t endptr_ptr = emu->get_reg(1);
    int base = (int)emu->get_reg(2);
    char* endptr = nullptr;
    long result = std::strtol((const char*)(memory + str), &endptr, base);
    if (endptr_ptr && endptr) {
        uint32_t offset = (uint32_t)(endptr - (char*)(memory + str));
        *(uint32_t*)(memory + endptr_ptr) = str + offset;
    }
    emu->set_reg(0, (uint32_t)result);
}

void bridge_strtoul(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t str = emu->get_reg(0);
    uint32_t endptr_ptr = emu->get_reg(1);
    int base = (int)emu->get_reg(2);
    char* endptr = nullptr;
    unsigned long result = std::strtoul((const char*)(memory + str), &endptr, base);
    if (endptr_ptr && endptr) {
        uint32_t offset = (uint32_t)(endptr - (char*)(memory + str));
        *(uint32_t*)(memory + endptr_ptr) = str + offset;
    }
    emu->set_reg(0, (uint32_t)result);
}

void bridge_atoi(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t str = emu->get_reg(0);
    emu->set_reg(0, (uint32_t)std::atoi((const char*)(memory + str)));
}

void bridge_atof(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t str = emu->get_reg(0);
    float f = (float)std::atof((const char*)(memory + str));
    emu->set_reg(0, float_to_uint(f));
}

// --- File I/O Bridges ---
static std::unordered_map<uint32_t, FILE*> g_file_handles;
static uint32_t g_next_file_handle = 0x70000001;

void bridge_fopen(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t path_ptr = emu->get_reg(0);
    uint32_t mode_ptr = emu->get_reg(1);
    const char* path = (const char*)(memory + path_ptr);
    const char* mode = (const char*)(memory + mode_ptr);
    std::cout << "[File] fopen(\"" << path << "\", \"" << mode << "\")" << std::endl;
    FILE* f = fopen(path, mode);
    if (f) {
        uint32_t handle = g_next_file_handle++;
        g_file_handles[handle] = f;
        emu->set_reg(0, handle);
    } else {
        emu->set_reg(0, 0);
    }
}

void bridge_fclose(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t handle = emu->get_reg(0);
    if (g_file_handles.count(handle)) {
        fclose(g_file_handles[handle]);
        g_file_handles.erase(handle);
        emu->set_reg(0, 0);
    } else {
        emu->set_reg(0, -1);
    }
}

void bridge_fread(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t buf = emu->get_reg(0);
    uint32_t elem_size = emu->get_reg(1);
    uint32_t count = emu->get_reg(2);
    uint32_t handle = emu->get_reg(3);
    if (g_file_handles.count(handle)) {
        size_t read = fread(memory + buf, elem_size, count, g_file_handles[handle]);
        emu->set_reg(0, (uint32_t)read);
    } else {
        emu->set_reg(0, 0);
    }
}

void bridge_fwrite(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t buf = emu->get_reg(0);
    uint32_t elem_size = emu->get_reg(1);
    uint32_t count = emu->get_reg(2);
    uint32_t handle = emu->get_reg(3);
    if (g_file_handles.count(handle)) {
        size_t written = fwrite(memory + buf, elem_size, count, g_file_handles[handle]);
        emu->set_reg(0, (uint32_t)written);
    } else {
        emu->set_reg(0, 0);
    }
}

void bridge_fseek(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t handle = emu->get_reg(0);
    int32_t offset = (int32_t)emu->get_reg(1);
    int whence = (int)emu->get_reg(2);
    if (g_file_handles.count(handle)) {
        emu->set_reg(0, (uint32_t)fseek(g_file_handles[handle], offset, whence));
    } else {
        emu->set_reg(0, (uint32_t)-1);
    }
}

void bridge_ftell(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t handle = emu->get_reg(0);
    if (g_file_handles.count(handle)) {
        emu->set_reg(0, (uint32_t)ftell(g_file_handles[handle]));
    } else {
        emu->set_reg(0, (uint32_t)-1);
    }
}

// --- Directory / System Bridges ---
void bridge_mkdir(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t path_ptr = emu->get_reg(0);
    uint32_t mode = emu->get_reg(1);
    const char* path = (const char*)(memory + path_ptr);
    int result = mkdir(path, (mode_t)mode);
    emu->set_reg(0, (uint32_t)result);
}

void bridge_stat(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    // Return -1 (file not found) as a safe default
    emu->set_reg(0, (uint32_t)-1);
}

// --- printf / logging ---
void bridge_printf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t fmt_ptr = emu->get_reg(0);
    const char* fmt = (const char*)(memory + fmt_ptr);
    std::cout << "[guest printf] " << fmt;
    emu->set_reg(0, 0);
}

void bridge_android_log_print(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    // int __android_log_print(int prio, const char* tag, const char* fmt, ...)
    uint32_t tag_ptr = emu->get_reg(1);
    uint32_t fmt_ptr = emu->get_reg(2);
    const char* tag = (const char*)(memory + tag_ptr);
    const char* fmt = (const char*)(memory + fmt_ptr);
    std::cout << "[ALOG] " << tag << ": " << fmt << std::endl;
    emu->set_reg(0, 0);
}

void bridge_snprintf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t buf = emu->get_reg(0);
    uint32_t size = emu->get_reg(1);
    uint32_t fmt_ptr = emu->get_reg(2);
    const char* fmt = (const char*)(memory + fmt_ptr);
    // Simple: just copy the format string as-is (no varargs handling)
    std::strncpy((char*)(memory + buf), fmt, size);
    if (size > 0) memory[buf + size - 1] = '\0';
    emu->set_reg(0, (uint32_t)std::strlen((char*)(memory + buf)));
}

void bridge_sprintf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t buf = emu->get_reg(0);
    uint32_t fmt_ptr = emu->get_reg(1);
    const char* fmt = (const char*)(memory + fmt_ptr);
    // Simple: just copy the format string as-is
    std::strcpy((char*)(memory + buf), fmt);
    emu->set_reg(0, (uint32_t)std::strlen((char*)(memory + buf)));
}

// --- OpenAL Stubs ---
void bridge_alcOpenDevice(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0xA1C00001); // Fake device handle
}

void bridge_alcCreateContext(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0xA1C00002); // Fake context handle
}

void bridge_alcMakeContextCurrent(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 1); // ALC_TRUE
}

void bridge_alGetError(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0); // AL_NO_ERROR
}

void bridge_alGenSources(void* emu_ptr) {
    // alGenSources(n, sources) - fill with fake source IDs
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t n = emu->get_reg(0);
    uint32_t sources_ptr = emu->get_reg(1);
    static uint32_t next_source = 1;
    for (uint32_t i = 0; i < n && i < 64; i++) {
        *(uint32_t*)(memory + sources_ptr + i * 4) = next_source++;
    }
}

void bridge_alGenBuffers(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t n = emu->get_reg(0);
    uint32_t buffers_ptr = emu->get_reg(1);
    static uint32_t next_buffer = 0x1000;
    for (uint32_t i = 0; i < n && i < 64; i++) {
        *(uint32_t*)(memory + buffers_ptr + i * 4) = next_buffer++;
    }
}

void bridge_al_noop(void* emu_ptr) {
    // Generic no-op for AL calls that don't need return values
}

// --- GLES Real Bridge Functions ---
// When g_display_active, these call real host OpenGL.
// Guest memory pointers are translated via (memory + guest_offset).

// Diagnostic: log GL calls on first 3 frames to debug black screen
int g_gl_diag_frame = 0;
bool g_gl_diag_enabled = true;
#define GL_DIAG(...) do { if (g_gl_diag_enabled && g_gl_diag_frame < 3) { printf("[GL F%d] ", g_gl_diag_frame); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while(0)
#define TEX_LOG(...) do { if (g_gl_diag_enabled && g_gl_diag_frame < 3) { printf("[TEX F%d] ", g_gl_diag_frame); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while(0)
#define VERTEX_LOG(...) do { if (g_gl_diag_enabled && g_gl_diag_frame < 3) { printf("[VERTEX F%d] ", g_gl_diag_frame); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while(0)
#define MATRIX_LOG(...) do { if (g_gl_diag_enabled && g_gl_diag_frame < 3) { printf("[MATRIX F%d] ", g_gl_diag_frame); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while(0)
#define EGL_LOG(...) do { printf("[EGL] "); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while(0)

static std::unordered_set<uint32_t> g_matrix_dumped_ptrs;

static void log_matrix_once(Emulator* emu, uint32_t ptr, const GLfloat* m) {
    if (!g_gl_diag_enabled || g_gl_diag_frame >= 3) return;
    if (g_matrix_dumped_ptrs.count(ptr)) return;
    g_matrix_dumped_ptrs.insert(ptr);
    uint8_t* memory = emu->get_memory_base();
    MATRIX_LOG("glLoadMatrixf ptr=0x%x host=%p sp=0x%x", ptr, (void*)(memory + ptr), emu->get_reg(13));
    MATRIX_LOG("  row0 [%.4f %.4f %.4f %.4f]", m[0], m[1], m[2], m[3]);
    MATRIX_LOG("  row1 [%.4f %.4f %.4f %.4f]", m[4], m[5], m[6], m[7]);
    MATRIX_LOG("  row2 [%.4f %.4f %.4f %.4f]", m[8], m[9], m[10], m[11]);
    MATRIX_LOG("  row3 [%.4f %.4f %.4f %.4f]", m[12], m[13], m[14], m[15]);
}

static void log_vertex_sample(Emulator* emu, uint32_t ptr, uint32_t size, uint32_t type, uint32_t stride, const char* label) {
    if (!g_gl_diag_enabled || g_gl_diag_frame >= 3) return;
    VERTEX_LOG("%s ptr=0x%x size=%u stride=%d type=0x%x", label, ptr, size, stride, type);
    if (ptr == 0) {
        VERTEX_LOG("  -> NULL pointer (no vertex data)");
        return;
    }
    if (type != GL_FLOAT) {
        VERTEX_LOG("  -> non-float type, skipping float dump");
        return;
    }
    uint8_t* memory = emu->get_memory_base();
    const float* v = (const float*)(memory + ptr);
    if (size >= 3) {
        VERTEX_LOG("  v0=(%f,%f,%f)", v[0], v[1], v[2]);
        const float* v1 = (stride > 0 && stride != (uint32_t)(size * 4))
            ? (const float*)((const uint8_t*)v + stride)
            : v + size;
        VERTEX_LOG("  v1=(%f,%f,%f)", v1[0], v1[1], v1[2]);
    } else if (size == 2) {
        VERTEX_LOG("  v0=(%f,%f)", v[0], v[1]);
        const float* v1 = (stride > 0 && stride != 8)
            ? (const float*)((const uint8_t*)v + stride)
            : v + 2;
        VERTEX_LOG("  v1=(%f,%f)", v1[0], v1[1]);
    }
}

void bridge_gl_noop(void* emu_ptr) {
    g_frame_stats.state_changes++;
}


// --- Phase 1: Clear/Viewport ---

void bridge_gl_clear(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.clear_calls++;
    GL_DIAG("glClear(0x%x)", emu->get_reg(0));
    if (g_display_active) {
        glClear(emu->get_reg(0));
    }
}

void bridge_gl_clear_color(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) {
        uint8_t* mem = emu->get_memory_base();
        // Soft-float ABI: floats passed in R0-R3 as uint32_t
        uint32_t r = emu->get_reg(0), g = emu->get_reg(1);
        uint32_t b = emu->get_reg(2), a = emu->get_reg(3);
        float fr, fg, fb, fa;
        memcpy(&fr, &r, 4); memcpy(&fg, &g, 4);
        memcpy(&fb, &b, 4); memcpy(&fa, &a, 4);
        GL_DIAG("glClearColor(%f, %f, %f, %f)", fr, fg, fb, fa);
        glClearColor(fr, fg, fb, fa);
    }
}

void bridge_gl_viewport(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.viewport_calls++;
    GL_DIAG("glViewport(%d, %d, %d, %d)", emu->get_reg(0), emu->get_reg(1), emu->get_reg(2), emu->get_reg(3));
    if (g_display_active) {
        glViewport(emu->get_reg(0), emu->get_reg(1), emu->get_reg(2), emu->get_reg(3));
    }
}

// --- Phase 2: Matrix Pipeline ---

void bridge_gl_matrix_mode(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.matrix_ops++;
    GL_DIAG("glMatrixMode(0x%x) %s", emu->get_reg(0), emu->get_reg(0)==0x1701?"PROJECTION":emu->get_reg(0)==0x1700?"MODELVIEW":"OTHER");
    if (g_display_active) {
        glMatrixMode(emu->get_reg(0));
    }
}

void bridge_gl_load_identity(void* emu_ptr) {
    g_frame_stats.matrix_ops++;
    if (g_display_active) glLoadIdentity();
}

void bridge_gl_load_matrixf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.matrix_ops++;
    uint8_t* memory = emu->get_memory_base();
    uint32_t ptr = emu->get_reg(0);
    const GLfloat* m = (const GLfloat*)(memory + ptr);
    log_matrix_once(emu, ptr, m);
    GL_DIAG("glLoadMatrixf(@0x%x) [%.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f / ...]", ptr, m[0],m[1],m[2],m[3],m[4],m[5],m[6],m[7]);
    if (g_display_active) {
        // Fix: If matrix is zeroed, use identity to prevent black screen
        if (m[0] == 0.0f && m[5] == 0.0f && m[10] == 0.0f) {
             GL_DIAG("  [FIX] Detected zero matrix, using identity");
             glLoadIdentity();
        } else {
             glLoadMatrixf(m);
        }
    }
}

void bridge_gl_mult_matrixf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.matrix_ops++;
    uint8_t* memory = emu->get_memory_base();
    const GLfloat* m = (const GLfloat*)(memory + emu->get_reg(0));
    GL_DIAG("glMultMatrixf(@0x%x) [%.2f %.2f %.2f %.2f / ...]", emu->get_reg(0), m[0],m[1],m[2],m[3]);
    if (g_display_active) {
        glMultMatrixf(m);
    }
}

void bridge_gl_push_matrix(void* emu_ptr) {
    g_frame_stats.matrix_ops++;
    GL_DIAG("glPushMatrix()");
    if (g_display_active) glPushMatrix();
}

void bridge_gl_pop_matrix(void* emu_ptr) {
    g_frame_stats.matrix_ops++;
    GL_DIAG("glPopMatrix()");
    if (g_display_active) glPopMatrix();
}

void bridge_gl_orthof(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.matrix_ops++;
    // Soft-float: 6 floats in R0-R3 + stack
    uint32_t r0 = emu->get_reg(0), r1 = emu->get_reg(1);
    uint32_t r2 = emu->get_reg(2), r3 = emu->get_reg(3);
    float l, r_, b, t;
    memcpy(&l, &r0, 4); memcpy(&r_, &r1, 4);
    memcpy(&b, &r2, 4); memcpy(&t, &r3, 4);
    // near/far on stack
    uint8_t* memory = emu->get_memory_base();
    uint32_t sp = emu->get_reg(13);
    float n, f;
    memcpy(&n, memory + sp, 4);
    memcpy(&f, memory + sp + 4, 4);
    GL_DIAG("glOrthof(l=%f, r=%f, b=%f, t=%f, n=%f, f=%f)", l, r_, b, t, n, f);
    if (g_display_active) {
        glOrtho(l, r_, b, t, n, f);
    }
}


void bridge_gl_translatef(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.matrix_ops++;
    if (g_display_active) {
        uint32_t r0 = emu->get_reg(0), r1 = emu->get_reg(1), r2 = emu->get_reg(2);
        float x, y, z;
        memcpy(&x, &r0, 4); memcpy(&y, &r1, 4); memcpy(&z, &r2, 4);
        glTranslatef(x, y, z);
    }
}

void bridge_gl_rotatef(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.matrix_ops++;
    if (g_display_active) {
        uint32_t r0 = emu->get_reg(0), r1 = emu->get_reg(1);
        uint32_t r2 = emu->get_reg(2), r3 = emu->get_reg(3);
        float angle, x, y, z;
        memcpy(&angle, &r0, 4); memcpy(&x, &r1, 4);
        memcpy(&y, &r2, 4); memcpy(&z, &r3, 4);
        glRotatef(angle, x, y, z);
    }
}

void bridge_gl_scalef(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.matrix_ops++;
    if (g_display_active) {
        uint32_t r0 = emu->get_reg(0), r1 = emu->get_reg(1), r2 = emu->get_reg(2);
        float x, y, z;
        memcpy(&x, &r0, 4); memcpy(&y, &r1, 4); memcpy(&z, &r2, 4);
        glScalef(x, y, z);
    }
}

void bridge_gl_frustumf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.matrix_ops++;
    if (g_display_active) {
        uint32_t r0 = emu->get_reg(0), r1 = emu->get_reg(1);
        uint32_t r2 = emu->get_reg(2), r3 = emu->get_reg(3);
        float l, r_, b, t;
        memcpy(&l, &r0, 4); memcpy(&r_, &r1, 4);
        memcpy(&b, &r2, 4); memcpy(&t, &r3, 4);
        uint8_t* memory = emu->get_memory_base();
        uint32_t sp = emu->get_reg(13);
        float n, f;
        memcpy(&n, memory + sp, 4);
        memcpy(&f, memory + sp + 4, 4);
        glFrustum(l, r_, b, t, n, f);
    }
}

// --- Phase 3: Geometry Submission ---

// Current GL state for debugging
struct {
    uint32_t vptr_addr;
    GLint vptr_size;
    GLenum vptr_type;
    GLsizei vptr_stride;
    
    uint32_t tptr_addr;
    GLint tptr_size;
    GLenum tptr_type;
    GLsizei tptr_stride;

    uint32_t nptr_addr;
    GLenum nptr_type;
    GLsizei nptr_stride;
} g_gl_state;

bool g_gl_force_white = false;
bool g_gl_force_identity = false;

void bridge_gl_draw_arrays(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t mode = emu->get_reg(0);
    uint32_t first = emu->get_reg(1);
    uint32_t count = emu->get_reg(2);
    g_frame_stats.draw_calls++;
    g_frame_stats.vertices_submitted += count;
    
    if (g_gl_diag_enabled && g_gl_diag_frame < 3) {
        printf("[GL F%d] glDrawArrays(mode=0x%x, first=%d, count=%d)\n", g_gl_diag_frame, mode, first, count);
        uint8_t* memory = emu->get_memory_base();
        if (g_gl_state.vptr_addr && g_gl_state.vptr_type == GL_FLOAT) {
            const float* v = (const float*)(memory + g_gl_state.vptr_addr + first * g_gl_state.vptr_size * 4);
            printf("  -> Verts: [%.2f %.2f %.2f / %.2f %.2f %.2f / ...]\n", v[0], v[1], v[2], v[3], v[4], v[5]);
        }
    }

    if (g_display_active) {
        if (g_gl_force_identity) {
            glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, 800, 480, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        }
        if (g_gl_force_white) {
            glDisable(GL_TEXTURE_2D);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        glDrawArrays(mode, first, count);

        if (g_gl_force_identity) {
            glMatrixMode(GL_PROJECTION); glPopMatrix();
            glMatrixMode(GL_MODELVIEW); glPopMatrix();
        }
    }
}

void bridge_gl_draw_elements(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t mode = emu->get_reg(0);
    uint32_t count = emu->get_reg(1);
    uint32_t type = emu->get_reg(2);
    uint32_t indices_ptr = emu->get_reg(3);
    g_frame_stats.draw_calls++;
    g_frame_stats.vertices_submitted += count;
    
    if (g_gl_diag_enabled && g_gl_diag_frame < 3) {
        printf("[GL F%d] glDrawElements(mode=0x%x, count=%d, type=0x%x, indices=@0x%x)\n", g_gl_diag_frame, mode, count, type, indices_ptr);
        uint8_t* memory = emu->get_memory_base();
        if (g_gl_state.vptr_addr && g_gl_state.vptr_type == GL_FLOAT) {
            const float* v = (const float*)(memory + g_gl_state.vptr_addr);
            printf("  -> Verts: [%.2f %.2f %.2f / %.2f %.2f %.2f / ...]\n", v[0], v[1], v[2], v[3], v[4], v[5]);
        }
    }

    if (g_display_active) {
        uint8_t* memory = emu->get_memory_base();
        if (g_gl_force_identity) {
            glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, 800, 480, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        }
        if (g_gl_force_white) {
            glDisable(GL_TEXTURE_2D);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        glDrawElements(mode, count, type, (const void*)(memory + indices_ptr));

        if (g_gl_force_identity) {
            glMatrixMode(GL_PROJECTION); glPopMatrix();
            glMatrixMode(GL_MODELVIEW); glPopMatrix();
        }
    }
}


// glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
void bridge_gl_vertex_pointer(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.vertex_pointer_calls++;
    
    uint32_t size = emu->get_reg(0);
    uint32_t type = emu->get_reg(1);
    uint32_t stride = emu->get_reg(2);
    uint32_t ptr = emu->get_reg(3);
    
    g_gl_state.vptr_addr = ptr;
    g_gl_state.vptr_size = size;
    g_gl_state.vptr_type = type;
    g_gl_state.vptr_stride = stride;

    log_vertex_sample(emu, ptr, size, type, stride, "glVertexPointer");
    if (g_display_active) {
        uint8_t* memory = emu->get_memory_base();
        GL_DIAG("glVertexPointer(size=%d, type=0x%x, stride=%d, ptr=0x%x -> host=%p)", size, type, stride, ptr, (void*)(memory + ptr));
        glVertexPointer(size, type, stride, (const void*)(memory + ptr));
    }
}


void bridge_gl_texcoord_pointer(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.texcoord_pointer_calls++;
    
    uint32_t size = emu->get_reg(0);
    uint32_t type = emu->get_reg(1);
    uint32_t stride = emu->get_reg(2);
    uint32_t ptr = emu->get_reg(3);
    
    g_gl_state.tptr_addr = ptr;
    g_gl_state.tptr_size = size;
    g_gl_state.tptr_type = type;
    g_gl_state.tptr_stride = stride;

    log_vertex_sample(emu, ptr, size, type, stride, "glTexCoordPointer");
    if (g_display_active) {
        uint8_t* memory = emu->get_memory_base();
        GL_DIAG("glTexCoordPointer(size=%d, type=0x%x, stride=%d, ptr=0x%x -> host=%p)", size, type, stride, ptr, (void*)(memory + ptr));
        glTexCoordPointer(size, type, stride, (const void*)(memory + ptr));
    }
}

void bridge_gl_color_pointer(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.color_pointer_calls++;
    if (g_display_active) {
        uint8_t* memory = emu->get_memory_base();
        uint32_t size = emu->get_reg(0);
        uint32_t type = emu->get_reg(1);
        uint32_t stride = emu->get_reg(2);
        uint32_t ptr = emu->get_reg(3);
        GL_DIAG("glColorPointer(size=%d, type=0x%x, stride=%d, ptr=0x%x -> host=%p)", size, type, stride, ptr, (void*)(memory + ptr));
        glColorPointer(size, type, stride, (const void*)(memory + ptr));
    }
}

void bridge_gl_normal_pointer(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    
    uint32_t type = emu->get_reg(0);
    uint32_t stride = emu->get_reg(1);
    uint32_t ptr = emu->get_reg(2);
    
    g_gl_state.nptr_addr = ptr;
    g_gl_state.nptr_type = type;
    g_gl_state.nptr_stride = stride;

    if (g_display_active) {
        uint8_t* memory = emu->get_memory_base();
        glNormalPointer(type, stride, (const void*)(memory + ptr));
    }
}

void bridge_gl_enable_client_state(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    GL_DIAG("glEnableClientState(0x%x) %s", emu->get_reg(0), emu->get_reg(0)==0x8074?"VERTEX_ARRAY":emu->get_reg(0)==0x8078?"TEXTURE_COORD_ARRAY":"OTHER");
    if (g_display_active) glEnableClientState(emu->get_reg(0));
}

void bridge_gl_disable_client_state(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    GL_DIAG("glDisableClientState(0x%x)", emu->get_reg(0));
    if (g_display_active) glDisableClientState(emu->get_reg(0));
}

// --- Phase 4: Textures ---

static std::unordered_map<uint32_t, bool> g_seen_textures;

extern bool g_gl_force_white;

void bridge_gl_bind_texture(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint32_t target = emu->get_reg(0);
    uint32_t tex_id = emu->get_reg(1);
    g_frame_stats.texture_binds++;
    g_frame_stats.last_bound_texture = tex_id;
    if (tex_id && !g_seen_textures.count(tex_id)) {
        g_seen_textures[tex_id] = true;
        g_frame_stats.unique_textures_bound++;
    }
    GL_DIAG("glBindTexture(target=0x%x, id=%d)", target, tex_id);
    TEX_LOG("glBindTexture(target=0x%x, id=%u)", target, tex_id);
    if (g_display_active) {
        glBindTexture(target, tex_id);
        if (g_gl_force_white) {
            if (tex_id == 0) {
                glDisable(GL_TEXTURE_2D);
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            } else {
                glEnable(GL_TEXTURE_2D);
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }
    }
}

// glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels)
// R0=target, R1=level, R2=internalformat, R3=width, stack: height, border, format, type, pixels
void bridge_gl_tex_image_2d(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.tex_uploads++;
    uint8_t* memory = emu->get_memory_base();
    uint32_t target = emu->get_reg(0);
    uint32_t level = emu->get_reg(1);
    uint32_t internalformat = emu->get_reg(2);
    uint32_t width = emu->get_reg(3);
    uint32_t sp = emu->get_reg(13);
    uint32_t height = *(uint32_t*)(memory + sp);
    uint32_t border = *(uint32_t*)(memory + sp + 4);
    uint32_t format = *(uint32_t*)(memory + sp + 8);
    uint32_t type = *(uint32_t*)(memory + sp + 12);
    uint32_t pixels_ptr = *(uint32_t*)(memory + sp + 16);
    TEX_LOG("glTexImage2D(%ux%u level=%u ifmt=0x%x fmt=0x%x type=0x%x pixels=0x%x)",
            width, height, level, internalformat, format, type, pixels_ptr);
    if (g_display_active) {
        const void* pixels = pixels_ptr ? (const void*)(memory + pixels_ptr) : nullptr;
        glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    }
}

void bridge_gl_tex_sub_image_2d(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.tex_uploads++;
    uint8_t* memory = emu->get_memory_base();
    uint32_t target = emu->get_reg(0);
    uint32_t level = emu->get_reg(1);
    uint32_t xoff = emu->get_reg(2);
    uint32_t yoff = emu->get_reg(3);
    uint32_t sp = emu->get_reg(13);
    uint32_t width = *(uint32_t*)(memory + sp);
    uint32_t height = *(uint32_t*)(memory + sp + 4);
    uint32_t format = *(uint32_t*)(memory + sp + 8);
    uint32_t type = *(uint32_t*)(memory + sp + 12);
    uint32_t pixels_ptr = *(uint32_t*)(memory + sp + 16);
    TEX_LOG("glTexSubImage2D(%ux%u @%u,%u level=%u fmt=0x%x type=0x%x pixels=0x%x)",
            width, height, xoff, yoff, level, format, type, pixels_ptr);
    if (g_display_active) {
        const void* pixels = pixels_ptr ? (const void*)(memory + pixels_ptr) : nullptr;
        glTexSubImage2D(target, level, xoff, yoff, width, height, format, type, pixels);
    }
}

void bridge_gl_compressed_tex_image_2d(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.tex_uploads++;
    uint8_t* memory = emu->get_memory_base();
    uint32_t width = emu->get_reg(3);
    uint32_t sp = emu->get_reg(13);
    uint32_t height = *(uint32_t*)(memory + sp);
    uint32_t image_size = *(uint32_t*)(memory + sp + 12);
    uint32_t data_ptr = *(uint32_t*)(memory + sp + 16);
    TEX_LOG("glCompressedTexImage2D(%ux%u size=%u data=0x%x) [skipped — no desktop decoder]",
            width, height, image_size, data_ptr);
    // Compressed textures (ETC1/PVRTC) are not natively supported on desktop GL.
    // We'll skip these for now — the engine will fall back to PNG.
}

void bridge_gl_tex_parameteri(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) {
        glTexParameteri(emu->get_reg(0), emu->get_reg(1), emu->get_reg(2));
    }
}

void bridge_gl_tex_parameterf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) {
        uint32_t r2 = emu->get_reg(2);
        float val; memcpy(&val, &r2, 4);
        glTexParameterf(emu->get_reg(0), emu->get_reg(1), val);
    }
}

// --- Phase 5: State & Blending ---

void bridge_gl_enable(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    GL_DIAG("glEnable(0x%x)", emu->get_reg(0));
    if (g_display_active) glEnable(emu->get_reg(0));
}

void bridge_gl_disable(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    GL_DIAG("glDisable(0x%x)", emu->get_reg(0));
    if (g_display_active) glDisable(emu->get_reg(0));
}

void bridge_gl_blend_func(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) glBlendFunc(emu->get_reg(0), emu->get_reg(1));
}

void bridge_gl_depth_func(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) glDepthFunc(emu->get_reg(0));
}

void bridge_gl_depth_mask(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) glDepthMask(emu->get_reg(0));
}

void bridge_gl_color4f(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) {
        uint32_t r0 = emu->get_reg(0), r1 = emu->get_reg(1);
        uint32_t r2 = emu->get_reg(2), r3 = emu->get_reg(3);
        float r, g, b, a;
        memcpy(&r, &r0, 4); memcpy(&g, &r1, 4);
        memcpy(&b, &r2, 4); memcpy(&a, &r3, 4);
        GL_DIAG("glColor4f(%f, %f, %f, %f)", r, g, b, a);
        glColor4f(r, g, b, a);
    }
}

void bridge_gl_scissor(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) glScissor(emu->get_reg(0), emu->get_reg(1), emu->get_reg(2), emu->get_reg(3));
}

void bridge_gl_color_mask(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) glColorMask(emu->get_reg(0), emu->get_reg(1), emu->get_reg(2), emu->get_reg(3));
}

void bridge_gl_pixel_storei(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) glPixelStorei(emu->get_reg(0), emu->get_reg(1));
}

void bridge_gl_alpha_func(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) {
        uint32_t r1 = emu->get_reg(1);
        float ref; memcpy(&ref, &r1, 4);
        glAlphaFunc(emu->get_reg(0), ref);
    }
}

void bridge_gl_shade_model(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) glShadeModel(emu->get_reg(0));
}

void bridge_gl_clear_depthf(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) {
        uint32_t r0 = emu->get_reg(0);
        float d; memcpy(&d, &r0, 4);
        glClearDepth(d);
    }
}

void bridge_gl_line_width(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) {
        uint32_t r0 = emu->get_reg(0);
        float w; memcpy(&w, &r0, 4);
        glLineWidth(w);
    }
}

void bridge_gl_active_texture(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) glActiveTexture(emu->get_reg(0));
}

// --- Queries & Gen ---

// eglGetProcAddress(const char* procname) -> void* func
void bridge_eglGetProcAddress(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t name_ptr = emu->get_reg(0);
    const char* procname = name_ptr ? (const char*)(memory + name_ptr) : "";
    EGL_LOG("eglGetProcAddress(\"%s\")", procname);
    uint32_t addr = 0;
    if (emu->bridge && procname[0]) {
        addr = emu->bridge->lookup_proc_address(procname);
    }
    if (addr) {
        EGL_LOG("  -> bridge 0x%x (%s)", addr, emu->bridge->get_name(addr).c_str());
    } else {
        EGL_LOG("  -> NULL (no bridge handler)");
    }
    emu->set_reg(0, addr);
}

void bridge_glGetError(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    if (g_display_active) {
        emu->set_reg(0, glGetError());
    } else {
        emu->set_reg(0, 0); // GL_NO_ERROR
    }
}

void bridge_glGenTextures(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t n = emu->get_reg(0);
    uint32_t textures_ptr = emu->get_reg(1);
    GL_DIAG("glGenTextures(n=%d)", n);
    TEX_LOG("glGenTextures(n=%u out=0x%x)", n, textures_ptr);
    if (g_display_active) {
        // Generate real GL texture IDs
        GLuint textures[64];
        if (n > 64) n = 64;
        glGenTextures(n, textures);
        for (uint32_t i = 0; i < n; i++) {
            *(uint32_t*)(memory + textures_ptr + i * 4) = textures[i];
            GL_DIAG("  -> id[%d] = %d", i, textures[i]);
            TEX_LOG("  -> id[%u] = %u", i, textures[i]);
        }
    } else {
        static uint32_t next_tex = 1;
        for (uint32_t i = 0; i < n && i < 64; i++) {
            *(uint32_t*)(memory + textures_ptr + i * 4) = next_tex++;
            GL_DIAG("  -> id[%d] = %d", i, next_tex - 1);
            TEX_LOG("  -> id[%u] = %u (stub)", i, next_tex - 1);
        }
    }
}

void bridge_glGenBuffers(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t n = emu->get_reg(0);
    uint32_t buffers_ptr = emu->get_reg(1);
    if (g_display_active) {
        GLuint buffers[64];
        if (n > 64) n = 64;
        glGenBuffers(n, buffers);
        for (uint32_t i = 0; i < n; i++) {
            *(uint32_t*)(memory + buffers_ptr + i * 4) = buffers[i];
        }
    } else {
        static uint32_t next_buf = 0x2000;
        for (uint32_t i = 0; i < n && i < 64; i++) {
            *(uint32_t*)(memory + buffers_ptr + i * 4) = next_buf++;
        }
    }
}

void bridge_glGetIntegerv(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t pname = emu->get_reg(0);
    uint32_t params_ptr = emu->get_reg(1);
    if (g_display_active) {
        glGetIntegerv(pname, (GLint*)(memory + params_ptr));
        GLint* vals = (GLint*)(memory + params_ptr);
        GL_DIAG("glGetIntegerv(pname=0x%x) -> [%d, %d, %d, %d]", pname, vals[0], vals[1], vals[2], vals[3]);
    } else {
        *(uint32_t*)(memory + params_ptr) = 2048;
    }
}

void bridge_glGetString(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    // Return a pointer to a string in guest memory
    static bool initialized = false;
    uint8_t* memory = emu->get_memory_base();
    if (!initialized) {
        strcpy((char*)(memory + 0x40000), "OpenGL ES 2.0 (Swordigo Desktop)");
        strcpy((char*)(memory + 0x40100), "Swordigo Desktop Emulator");
        strcpy((char*)(memory + 0x40200), "GL_OES_texture_npot GL_OES_compressed_ETC1_RGB8_texture");
        initialized = true;
    }
    uint32_t name = emu->get_reg(0);
    switch (name) {
        case 0x1F00: emu->set_reg(0, 0x40100); break; // GL_VENDOR
        case 0x1F01: emu->set_reg(0, 0x40100); break; // GL_RENDERER
        case 0x1F02: emu->set_reg(0, 0x40000); break; // GL_VERSION
        case 0x1F03: emu->set_reg(0, 0x40200); break; // GL_EXTENSIONS
        default: emu->set_reg(0, 0x40000); break;
    }
}

// --- Misc GL stubs (things that need GL but are less critical) ---

void bridge_gl_delete_textures(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    if (g_display_active) {
        uint8_t* memory = emu->get_memory_base();
        uint32_t n = emu->get_reg(0);
        uint32_t textures_ptr = emu->get_reg(1);
        glDeleteTextures(n, (const GLuint*)(memory + textures_ptr));
    }
}

void bridge_gl_delete_buffers(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    if (g_display_active) {
        uint8_t* memory = emu->get_memory_base();
        uint32_t n = emu->get_reg(0);
        uint32_t buffers_ptr = emu->get_reg(1);
        glDeleteBuffers(n, (const GLuint*)(memory + buffers_ptr));
    }
}

void bridge_gl_bind_buffer(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) glBindBuffer(emu->get_reg(0), emu->get_reg(1));
}

void bridge_gl_buffer_data(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    g_frame_stats.state_changes++;
    if (g_display_active) {
        uint8_t* memory = emu->get_memory_base();
        uint32_t target = emu->get_reg(0);
        uint32_t size = emu->get_reg(1);
        uint32_t data_ptr = emu->get_reg(2);
        uint32_t usage = emu->get_reg(3);
        const void* data = data_ptr ? (const void*)(memory + data_ptr) : nullptr;
        glBufferData(target, size, data, usage);
    }
}

void bridge_gl_flush(void* emu_ptr) {
    if (g_display_active) glFlush();
}

void bridge_gl_finish(void* emu_ptr) {
    if (g_display_active) glFinish();
}

void bridge_gl_framebuffer_status(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0x8CD5); // GL_FRAMEBUFFER_COMPLETE
}


// --- Misc stubs ---
// __errno returns a pointer to the errno variable in guest memory
static const uint32_t GUEST_ERRNO_ADDR = 0x41000; // Reserved area
void bridge_errno(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, GUEST_ERRNO_ADDR);
}

void bridge_google_blocking(void* emu_ptr) {
    // __google_potentially_blocking_region_begin/end - no-op
}

void bridge_cxa_atexit(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0);
}


void bridge_stack_chk_fail(void* emu_ptr) {
    std::cerr << "[FATAL] __stack_chk_fail called!" << std::endl;
}

void bridge_cxa_guard(void* emu_ptr) {
    // __cxa_guard_acquire/release/abort - just succeed
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 1);
}

void bridge_pthread_mutex(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0); // success
}

void bridge_pthread_cond(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 0); // success
}

void bridge_pthread_create(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    // Can't actually create threads in guest - return error
    std::cout << "[Bridge] pthread_create called (stubbed)" << std::endl;
    emu->set_reg(0, 0); // pretend success
}

void bridge_pthread_self(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    emu->set_reg(0, 1); // fake thread id
}

void bridge_time(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t t_ptr = emu->get_reg(0);
    time_t t = time(nullptr);
    if (t_ptr) {
        *(uint32_t*)(memory + t_ptr) = (uint32_t)t;
    }
    emu->set_reg(0, (uint32_t)t);
}

void bridge_clock_gettime(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t tp_ptr = emu->get_reg(1);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (tp_ptr) {
        *(uint32_t*)(memory + tp_ptr) = (uint32_t)ts.tv_sec;
        *(uint32_t*)(memory + tp_ptr + 4) = (uint32_t)ts.tv_nsec;
    }
    emu->set_reg(0, 0);
}

void bridge_gettimeofday(void* emu_ptr) {
    Emulator* emu = (Emulator*)emu_ptr;
    uint8_t* memory = emu->get_memory_base();
    uint32_t tv_ptr = emu->get_reg(0);
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    if (tv_ptr) {
        *(uint32_t*)(memory + tv_ptr) = (uint32_t)tv.tv_sec;
        *(uint32_t*)(memory + tv_ptr + 4) = (uint32_t)tv.tv_usec;
    }
    emu->set_reg(0, 0);
}

void JniBridge::init_standard_bridges() {
    register_handler("malloc", bridge_malloc);
    register_handler("calloc", bridge_calloc);
    register_handler("realloc", bridge_realloc);
    register_handler("free", bridge_free);

    register_handler("memcpy", bridge_memcpy);
    register_handler("memset", bridge_memset);
    register_handler("memmove", bridge_memmove);
    register_handler("strlen", bridge_strlen);
    register_handler("memcmp", bridge_memcmp);
    register_handler("memchr", bridge_memchr_impl);

    register_handler("strchr", bridge_strchr);
    register_handler("strrchr", bridge_strrchr);
    register_handler("strcpy", bridge_strcpy);
    register_handler("strncpy", bridge_strncpy);
    register_handler("strcmp", bridge_strcmp);
    register_handler("strncmp", bridge_strncmp);
    register_handler("strcat", bridge_strcat);
    register_handler("strstr", bridge_strstr);
    register_handler("strtol", bridge_strtol);
    register_handler("strtoul", bridge_strtoul);
    register_handler("atoi", bridge_atoi);
    register_handler("atof", bridge_atof);

    register_handler("__aeabi_memclr", bridge_aeabi_memclr);
    register_handler("__aeabi_memclr4", bridge_aeabi_memclr);
    register_handler("__aeabi_memclr8", bridge_aeabi_memclr);
    register_handler("__aeabi_memset", bridge_aeabi_memset);
    register_handler("__aeabi_memset4", bridge_aeabi_memset);
    register_handler("__aeabi_memset8", bridge_aeabi_memset);
    register_handler("__aeabi_memcpy", bridge_aeabi_memcpy);
    register_handler("__aeabi_memcpy4", bridge_aeabi_memcpy);
    register_handler("__aeabi_memcpy8", bridge_aeabi_memcpy);
    register_handler("__aeabi_memmove", bridge_aeabi_memmove);
    register_handler("__aeabi_memmove4", bridge_aeabi_memmove);
    register_handler("__aeabi_memmove8", bridge_aeabi_memmove);

    register_handler("__aeabi_uidiv", bridge_aeabi_uidiv);
    register_handler("__aeabi_idiv", bridge_aeabi_idiv);
    register_handler("__aeabi_uidivmod", bridge_aeabi_uidivmod);
    register_handler("__aeabi_idivmod", bridge_aeabi_idivmod);
    register_handler("__aeabi_ldivmod", bridge_aeabi_ldivmod);
    register_handler("__aeabi_uldivmod", bridge_aeabi_uldivmod);

    register_handler("cosf", bridge_cosf);
    register_handler("sinf", bridge_sinf);
    register_handler("tanf", bridge_tanf);
    register_handler("acosf", bridge_acosf);
    register_handler("asinf", bridge_asinf);
    register_handler("atanf", bridge_atanf);
    register_handler("atan2f", bridge_atan2f);
    register_handler("roundf", bridge_roundf);
    register_handler("floorf", bridge_floorf);
    register_handler("ceilf", bridge_ceilf);
    register_handler("sqrtf", bridge_sqrtf);

    register_handler("powf", bridge_powf);
    register_handler("sincosf", bridge_sincosf);

    // File I/O
    register_handler("fopen", bridge_fopen);
    register_handler("fclose", bridge_fclose);
    register_handler("fread", bridge_fread);
    register_handler("fwrite", bridge_fwrite);
    register_handler("fseek", bridge_fseek);
    register_handler("ftell", bridge_ftell);

    // Directory / system
    register_handler("mkdir", bridge_mkdir);
    register_handler("stat", bridge_stat);

    // Printf / logging
    register_handler("printf", bridge_printf);
    register_handler("snprintf", bridge_snprintf);
    register_handler("sprintf", bridge_sprintf);
    register_handler("__android_log_print", bridge_android_log_print);
    register_handler("__android_log_write", bridge_android_log_print);
    register_handler("__android_log_vprint", bridge_android_log_print);

    // Misc libc
    register_handler("__errno", bridge_errno);
    register_handler("__cxa_atexit", bridge_cxa_atexit);
    register_handler("__cxa_finalize", bridge_cxa_atexit);
    register_handler("__stack_chk_fail", bridge_stack_chk_fail);
    register_handler("__cxa_guard_acquire", bridge_cxa_guard);
    register_handler("__cxa_guard_release", bridge_cxa_guard);
    register_handler("__google_potentially_blocking_region_begin", bridge_google_blocking);
    register_handler("__google_potentially_blocking_region_end", bridge_google_blocking);
    register_handler("time", bridge_time);
    register_handler("clock_gettime", bridge_clock_gettime);
    register_handler("gettimeofday", bridge_gettimeofday);

    // pthreads
    register_handler("pthread_mutex_init", bridge_pthread_mutex);
    register_handler("pthread_mutex_lock", bridge_pthread_mutex);
    register_handler("pthread_mutex_unlock", bridge_pthread_mutex);
    register_handler("pthread_mutex_destroy", bridge_pthread_mutex);
    register_handler("pthread_cond_init", bridge_pthread_cond);
    register_handler("pthread_cond_signal", bridge_pthread_cond);
    register_handler("pthread_cond_broadcast", bridge_pthread_cond);
    register_handler("pthread_cond_wait", bridge_pthread_cond);
    register_handler("pthread_cond_destroy", bridge_pthread_cond);
    register_handler("pthread_create", bridge_pthread_create);
    register_handler("pthread_self", bridge_pthread_self);
    register_handler("pthread_once", bridge_pthread_mutex);

    // JNI
    register_handler("FindClass", bridge_FindClass);
    register_handler("GetMethodID", bridge_GetMethodID);
    register_handler("GetStaticMethodID", bridge_GetMethodID); 
    register_handler("RegisterNatives", bridge_RegisterNatives);
    register_handler("NewGlobalRef", bridge_NewGlobalRef);
    register_handler("NewStringUTF", bridge_NewStringUTF);
    register_handler("GetStringUTFChars", bridge_GetStringUTFChars);
    
    // Asset Manager
    register_handler("AAssetManager_fromJava", bridge_AAssetManager_fromJava);
    register_handler("AAssetManager_open", bridge_AAssetManager_open);
    register_handler("AAsset_read", bridge_AAsset_read);
    register_handler("AAsset_close", bridge_AAsset_close);
    register_handler("AAsset_getLength", bridge_AAsset_getLength);
    register_handler("AAsset_openFileDescriptor", bridge_AAsset_openFileDescriptor);

    // JNI Env
    register_handler("ThrowNew", bridge_ThrowNew);
    register_handler("PushLocalFrame", bridge_PushLocalFrame);
    register_handler("PopLocalFrame", bridge_PopLocalFrame);
    register_handler("DeleteLocalRef", bridge_DeleteLocalRef);
    register_handler("DeleteGlobalRef", bridge_DeleteGlobalRef);
    register_handler("NewObjectV", bridge_NewObjectV);
    register_handler("GetObjectClass", bridge_GetObjectClass);
    register_handler("CallObjectMethodV", bridge_CallObjectMethodV);
    register_handler("CallBooleanMethodV", bridge_CallBooleanMethodV);
    register_handler("CallIntMethodV", bridge_CallIntMethodV);
    register_handler("CallLongMethodV", bridge_CallLongMethodV);
    register_handler("CallVoidMethodV", bridge_CallVoidMethodV);
    register_handler("GetFieldID", bridge_GetFieldID);
    register_handler("GetBooleanField", bridge_GetBooleanField);
    register_handler("GetIntField", bridge_GetIntField);
    register_handler("GetFloatField", bridge_GetFloatField);
    register_handler("CallStaticObjectMethodV", bridge_CallStaticObjectMethodV);
    register_handler("CallStaticBooleanMethodV", bridge_CallStaticBooleanMethodV);
    register_handler("CallStaticIntMethodV", bridge_CallStaticIntMethodV);
    register_handler("CallStaticLongMethodV", bridge_CallStaticLongMethodV);
    register_handler("CallStaticFloatMethodV", bridge_CallStaticFloatMethodV);
    register_handler("CallStaticVoidMethodV", bridge_CallStaticVoidMethodV);
    register_handler("GetStaticFieldID", bridge_GetStaticFieldID);
    register_handler("GetStaticObjectField", bridge_GetStaticObjectField);
    register_handler("GetStringUTFLength", bridge_GetStringUTFLength);
    register_handler("ReleaseStringUTFChars", bridge_ReleaseStringUTFChars);
    register_handler("GetArrayLength", bridge_GetArrayLength);
    register_handler("GetObjectArrayElement", bridge_GetObjectArrayElement);
    register_handler("NewIntArray", bridge_NewIntArray);
    register_handler("GetIntArrayElements", bridge_GetIntArrayElements);
    register_handler("ReleaseIntArrayElements", bridge_ReleaseIntArrayElements);
    register_handler("SetIntArrayRegion", bridge_SetIntArrayRegion);
    register_handler("GetJavaVM", bridge_GetJavaVM);
    register_handler("GetEnv", bridge_GetEnv);
    register_handler("GetStringUTFRegion", bridge_GetStringUTFRegion);

    // OpenAL
    register_handler("alcOpenDevice", bridge_alcOpenDevice);
    register_handler("alcCreateContext", bridge_alcCreateContext);
    register_handler("alcMakeContextCurrent", bridge_alcMakeContextCurrent);
    register_handler("alGetError", bridge_alGetError);
    register_handler("alGenSources", bridge_alGenSources);
    register_handler("alGenBuffers", bridge_alGenBuffers);
    register_handler("alSourcePlay", bridge_al_noop);
    register_handler("alSourceStop", bridge_al_noop);
    register_handler("alSourcePause", bridge_al_noop);
    register_handler("alSourcei", bridge_al_noop);
    register_handler("alSourcef", bridge_al_noop);
    register_handler("alSource3f", bridge_al_noop);
    register_handler("alBufferData", bridge_al_noop);
    register_handler("alSourceQueueBuffers", bridge_al_noop);
    register_handler("alSourceUnqueueBuffers", bridge_al_noop);
    register_handler("alGetSourcei", bridge_al_noop);
    register_handler("alDeleteSources", bridge_al_noop);
    register_handler("alDeleteBuffers", bridge_al_noop);
    register_handler("alListenerf", bridge_al_noop);
    register_handler("alListener3f", bridge_al_noop);
    register_handler("alDistanceModel", bridge_al_noop);

    // GLES real bridge functions
    register_handler("glViewport", bridge_gl_viewport);
    register_handler("glClear", bridge_gl_clear);
    register_handler("glClearColor", bridge_gl_clear_color);
    register_handler("glEnable", bridge_gl_enable);
    register_handler("glDisable", bridge_gl_disable);
    register_handler("glBlendFunc", bridge_gl_blend_func);
    register_handler("glDrawArrays", bridge_gl_draw_arrays);
    register_handler("glDrawElements", bridge_gl_draw_elements);
    register_handler("glBindTexture", bridge_gl_bind_texture);
    register_handler("glTexImage2D", bridge_gl_tex_image_2d);
    register_handler("glTexSubImage2D", bridge_gl_tex_sub_image_2d);
    register_handler("glTexParameteri", bridge_gl_tex_parameteri);
    register_handler("glTexParameterf", bridge_gl_tex_parameterf);
    register_handler("glCompressedTexImage2D", bridge_gl_compressed_tex_image_2d);
    register_handler("glVertexPointer", bridge_gl_vertex_pointer);
    register_handler("glTexCoordPointer", bridge_gl_texcoord_pointer);
    register_handler("glColorPointer", bridge_gl_color_pointer);
    register_handler("glNormalPointer", bridge_gl_normal_pointer);
    register_handler("glEnableClientState", bridge_gl_enable_client_state);
    register_handler("glDisableClientState", bridge_gl_disable_client_state);
    register_handler("glMatrixMode", bridge_gl_matrix_mode);
    register_handler("glLoadIdentity", bridge_gl_load_identity);
    register_handler("glLoadMatrixf", bridge_gl_load_matrixf);
    register_handler("glMultMatrixf", bridge_gl_mult_matrixf);
    register_handler("glPushMatrix", bridge_gl_push_matrix);
    register_handler("glPopMatrix", bridge_gl_pop_matrix);
    register_handler("glOrthof", bridge_gl_orthof);
    register_handler("glFrustumf", bridge_gl_frustumf);
    register_handler("glTranslatef", bridge_gl_translatef);
    register_handler("glRotatef", bridge_gl_rotatef);
    register_handler("glScalef", bridge_gl_scalef);
    register_handler("glColor4f", bridge_gl_color4f);
    register_handler("glDepthFunc", bridge_gl_depth_func);
    register_handler("glDepthMask", bridge_gl_depth_mask);
    register_handler("glClearDepthf", bridge_gl_clear_depthf);
    register_handler("glLineWidth", bridge_gl_line_width);
    register_handler("glScissor", bridge_gl_scissor);
    register_handler("glColorMask", bridge_gl_color_mask);
    register_handler("glStencilFunc", bridge_gl_noop);
    register_handler("glStencilMask", bridge_gl_noop);
    register_handler("glStencilOp", bridge_gl_noop);
    register_handler("glActiveTexture", bridge_gl_active_texture);
    register_handler("glClientActiveTexture", bridge_gl_active_texture);
    register_handler("glPixelStorei", bridge_gl_pixel_storei);
    register_handler("glVertexAttribPointer", bridge_gl_noop);
    register_handler("glEnableVertexAttribArray", bridge_gl_noop);
    register_handler("glDisableVertexAttribArray", bridge_gl_noop);
    register_handler("glUseProgram", bridge_gl_noop);
    register_handler("glBindBuffer", bridge_gl_bind_buffer);
    register_handler("glBufferData", bridge_gl_buffer_data);
    register_handler("glBufferSubData", bridge_gl_noop);
    register_handler("glDeleteTextures", bridge_gl_delete_textures);
    register_handler("glDeleteBuffers", bridge_gl_delete_buffers);
    register_handler("glBindFramebuffer", bridge_gl_noop);
    register_handler("glGenFramebuffers", bridge_glGenBuffers);
    register_handler("glDeleteFramebuffers", bridge_gl_noop);
    register_handler("glFramebufferTexture2D", bridge_gl_noop);
    register_handler("glCheckFramebufferStatus", bridge_gl_framebuffer_status);
    register_handler("glGenTextures", bridge_glGenTextures);
    register_handler("glGenBuffers", bridge_glGenBuffers);
    register_handler("glGetError", bridge_glGetError);
    register_handler("glGetIntegerv", bridge_glGetIntegerv);
    register_handler("glGetString", bridge_glGetString);
    register_handler("glAlphaFunc", bridge_gl_alpha_func);
    register_handler("glShadeModel", bridge_gl_shade_model);
    register_handler("glFogf", bridge_gl_noop);
    register_handler("glFogfv", bridge_gl_noop);
    register_handler("glFogi", bridge_gl_noop);
    register_handler("glMaterialf", bridge_gl_noop);
    register_handler("glMaterialfv", bridge_gl_noop);
    register_handler("glLightf", bridge_gl_noop);
    register_handler("glLightfv", bridge_gl_noop);
    register_handler("glLightModelf", bridge_gl_noop);
    register_handler("glLightModelfv", bridge_gl_noop);
    register_handler("glPointSize", bridge_gl_noop);
    register_handler("glHint", bridge_gl_noop);
    register_handler("glFlush", bridge_gl_flush);
    register_handler("glFinish", bridge_gl_finish);
    register_handler("glReadPixels", bridge_gl_noop);

    // EGL
    register_handler("eglGetDisplay", bridge_gl_noop);
    register_handler("eglSwapBuffers", bridge_gl_noop);
    register_handler("eglGetProcAddress", bridge_eglGetProcAddress);
}



