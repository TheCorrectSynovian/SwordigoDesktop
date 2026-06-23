#include "platform/data_path.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <filesystem>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#endif

namespace fs = std::filesystem;

// ============================================================
//  User data directory: ~/.local/share/swordigo-desktop/
//  Like Minecraft's ~/.minecraft/ — writable, user-owned.
// ============================================================

static std::string s_user_data_dir_cache;
static std::string s_system_data_dir_cache;
static bool s_dirs_resolved = false;

static void resolve_dirs() {
    if (s_dirs_resolved) return;
    s_dirs_resolved = true;

#ifndef _WIN32
    // User data dir: $XDG_DATA_HOME/swordigo-desktop/ or ~/.local/share/swordigo-desktop/
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg && xdg[0]) {
        s_user_data_dir_cache = std::string(xdg) + "/swordigo-desktop/";
    } else {
        const char* home = getenv("HOME");
        if (!home) {
            struct passwd* pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        s_user_data_dir_cache = std::string(home) + "/.local/share/swordigo-desktop/";
    }

    // System install dir: check standard paths
    const char* sys_paths[] = {
        "/usr/share/swordigo/",
        "/usr/local/share/swordigo/",
        nullptr
    };
    for (int i = 0; sys_paths[i]; i++) {
        if (fs::exists(sys_paths[i])) {
            s_system_data_dir_cache = sys_paths[i];
            break;
        }
    }
#else
    // Windows fallback
    const char* appdata = getenv("APPDATA");
    s_user_data_dir_cache = std::string(appdata ? appdata : ".") + "/swordigo-desktop/";
#endif
}

std::string get_user_data_dir() {
    resolve_dirs();
    // Create if needed
    if (!s_user_data_dir_cache.empty()) {
        try { fs::create_directories(s_user_data_dir_cache); } catch (...) {}
    }
    return s_user_data_dir_cache;
}

// C-linkage wrapper for asset_manager.c (pure C can't call std::string functions)
static char s_data_dir_c[512] = {0};
extern "C" char* get_user_data_dir_c(void) {
    std::string dir = get_user_data_dir();
    strncpy(s_data_dir_c, dir.c_str(), sizeof(s_data_dir_c) - 1);
    return s_data_dir_c;
}

std::string get_system_data_dir() {
    resolve_dirs();
    return s_system_data_dir_cache;
}

// ============================================================
//  First-run setup: copy from /usr/share/swordigo/ → ~/.local/share/swordigo-desktop/
// ============================================================

bool ensure_user_data() {
    std::string user_dir = get_user_data_dir();
    std::string sys_dir = get_system_data_dir();

    if (user_dir.empty()) return false;

    // Check if user data already exists (has engine/ or assets/)
    if (fs::exists(user_dir + "engine/manifest.json") && fs::exists(user_dir + "assets/")) {
        std::cout << "[DataPath] User data exists at " << user_dir << std::endl;
        return false; // Already set up
    }

    // If no system install, check CWD (dev mode)
    if (sys_dir.empty()) {
        if (fs::exists("./engine/manifest.json") && fs::exists("./assets/")) {
            std::cout << "[DataPath] Running in dev mode (CWD has game data)" << std::endl;
            return false; // Dev mode — use CWD directly
        }
        std::cout << "[DataPath] No system install and no local data found" << std::endl;
        return false;
    }

    // ---- FIRST RUN: Copy system data to user dir ----
    std::cout << "============================================" << std::endl;
    std::cout << " First-run setup: copying game data..." << std::endl;
    std::cout << "   From: " << sys_dir << std::endl;
    std::cout << "   To:   " << user_dir << std::endl;
    std::cout << "============================================" << std::endl;

    try {
        fs::create_directories(user_dir);

        // Copy directories: engine/, assets/, res/, src/assets/ (launcher textures)
        const char* dirs_to_copy[] = { "engine", "assets", "res", "src/assets", nullptr };
        for (int i = 0; dirs_to_copy[i]; i++) {
            std::string src = sys_dir + dirs_to_copy[i];
            std::string dst = user_dir + dirs_to_copy[i];
            if (fs::exists(src)) {
                fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                int count = 0;
                for (auto& e : fs::recursive_directory_iterator(dst)) {
                    if (e.is_regular_file()) count++;
                }
                std::cout << "   ✓ " << dirs_to_copy[i] << "/ (" << count << " files)" << std::endl;
            }
        }

        // Create save/ and cache/ directories
        fs::create_directories(user_dir + "save");
        fs::create_directories(user_dir + "cache");

        std::cout << "   ✓ Setup complete!" << std::endl;
        std::cout << "============================================" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[DataPath] ERROR copying game data: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================
//  get_data_path: resolve relative path to actual file
// ============================================================

std::string get_data_path(const std::string& relative_path) {
    // 1. Environment variable override
    const char* env_dir = getenv("SWORDIGO_DATA_DIR");
    if (env_dir) {
        std::string base = env_dir;
        if (!base.empty() && base.back() != '/') base += "/";
        return base + relative_path;
    }

    // 2. Compile-time define
#ifdef SWORDIGO_DATA_DIR_PATH
    {
        std::string path = std::string(SWORDIGO_DATA_DIR_PATH) + "/" + relative_path;
        if (fs::exists(path)) return path;
    }
#endif

    // 3. User data directory (~/.local/share/swordigo-desktop/)
    {
        std::string user_dir = get_user_data_dir();
        if (!user_dir.empty()) {
            std::string path = user_dir + relative_path;
            if (fs::exists(path)) return path;
        }
    }

    // 4. Current directory (development mode)
    {
        std::string path = "./" + relative_path;
        if (fs::exists(path)) return path;
    }

#ifndef _WIN32
    // 5. Relative to binary location
    {
        char exe_buf[4096];
        ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
        if (len > 0) {
            exe_buf[len] = '\0';
            fs::path exe_dir = fs::path(exe_buf).parent_path();
            std::string path = (exe_dir / relative_path).string();
            if (fs::exists(path)) return path;
            path = (exe_dir.parent_path() / relative_path).string();
            if (fs::exists(path)) return path;
        }
    }

    // 6. System install paths (read-only fallback)
    {
        std::string sys_dir = get_system_data_dir();
        if (!sys_dir.empty()) {
            std::string path = sys_dir + relative_path;
            if (fs::exists(path)) return path;
        }
    }
#endif

    // Fallback
    return "./" + relative_path;
}
