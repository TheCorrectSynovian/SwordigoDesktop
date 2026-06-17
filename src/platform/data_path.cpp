#include "platform/data_path.h"
#include <cstdlib>
#include <unistd.h>
#include <string>

std::string get_data_path(const std::string& relative_path) {
    // 1. Environment variable (AppImage / manual override)
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
        if (access(path.c_str(), F_OK) == 0) return path;
    }
#endif

    // 3. Current directory (development mode)
    {
        std::string path = "./" + relative_path;
        if (access(path.c_str(), F_OK) == 0) return path;
    }

    // 4. Standard system install paths
    const char* sys_paths[] = {
        "/usr/share/swordigo/",
        "/usr/local/share/swordigo/",
        nullptr
    };
    for (int i = 0; sys_paths[i]; i++) {
        std::string path = std::string(sys_paths[i]) + relative_path;
        if (access(path.c_str(), F_OK) == 0) return path;
    }

    // Fallback to current directory
    return "./" + relative_path;
}
