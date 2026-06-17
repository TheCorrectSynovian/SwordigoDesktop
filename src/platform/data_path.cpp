#include "platform/data_path.h"
#include <cstdlib>
#include <string>
#include <filesystem>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;

std::string get_data_path(const std::string& relative_path) {
    // 1. Environment variable (AppImage / manual override)
    const char* env_dir = getenv("SWORDIGO_DATA_DIR");
    if (env_dir) {
        std::string base = env_dir;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') base += "/";
        return base + relative_path;
    }

    // 2. Compile-time define
#ifdef SWORDIGO_DATA_DIR_PATH
    {
        std::string path = std::string(SWORDIGO_DATA_DIR_PATH) + "/" + relative_path;
        if (fs::exists(path)) return path;
    }
#endif

    // 3. Current directory (development mode)
    {
        std::string path = "./" + relative_path;
        if (fs::exists(path)) return path;
    }

#ifndef _WIN32
    // 4. Relative to binary location (dev build: build/swordigo_boot → project root)
    {
        char exe_buf[4096];
        ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
        if (len > 0) {
            exe_buf[len] = '\0';
            fs::path exe_dir = fs::path(exe_buf).parent_path();
            // Check same directory as binary
            std::string path = (exe_dir / relative_path).string();
            if (fs::exists(path)) return path;
            // Check one level up (build/ → project root)
            path = (exe_dir.parent_path() / relative_path).string();
            if (fs::exists(path)) return path;
        }
    }

    // 5. Standard system install paths (Linux only)
    const char* sys_paths[] = {
        "/usr/share/swordigo/",
        "/usr/local/share/swordigo/",
        nullptr
    };
    for (int i = 0; sys_paths[i]; i++) {
        std::string path = std::string(sys_paths[i]) + relative_path;
        if (fs::exists(path)) return path;
    }
#endif

    // Fallback to current directory
    return "./" + relative_path;
}
