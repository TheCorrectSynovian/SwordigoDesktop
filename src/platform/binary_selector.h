#pragma once
#include <string>
#include <vector>
#include <map>

// Binary version status
enum class BinaryStatus {
    TESTED,     // Known stable
    TESTING,    // User is testing
    UNKNOWN     // Unrecognized hash
};

// CPU architecture
enum class BinaryArch {
    ARM32,      // armeabi-v7a
    ARM64       // arm64-v8a
};

// Info about a detected game binary
struct BinaryInfo {
    std::string filename;       // e.g. "libswordigo.so"
    std::string filepath;       // Full path: "engine/v1.4.12/arm64-v8a/libswordigo.so"
    std::string sha256;         // SHA256 hex string
    std::string version;        // e.g. "1.4.12"
    std::string label;          // e.g. "v1.4.12 [ARM64] (Stable)"
    std::string version_dir;    // e.g. "v1.4.12" or "rl-v6.1"
    BinaryStatus status;
    BinaryArch arch;
    size_t file_size;           // bytes
    bool is_default;
    std::string game_type;      // "Swordigo" or "RLSwordigo"
    std::string assets_dir;     // "assets" or "rl_assets"
    std::string icon_path;      // Custom icon PNG/JPG (empty = use default for game_type)
    std::vector<std::string> dependencies;  // e.g. {"libmini.so", "libGlossHook.so"}
    std::vector<std::string> dep_paths;     // Full paths to dependency .so files
};

// Manages binary detection, hashing, and selection
class BinarySelector {
public:
    BinarySelector();

    // ── JSON Manifest System ──
    // Primary: load from pre-built JSON manifests (no filesystem scanning at boot)
    //   system_manifest = /usr/share/swordigo/engine/manifest.json (shipped with RPM)
    //   user_instances   = ~/.config/swordigo-desktop/instances.json (user-added)
    
    // Load system manifest (read-only, shipped with package)
    void load_manifest(const std::string& manifest_path);
    
    // Load user-added instances (writable, from user config dir)
    void load_user_instances(const std::string& json_path);
    
    // Save user instances (only custom ones, not system ones)
    void save_user_instances(const std::string& json_path) const;
    
    // Generate manifest.json from current engine/ directory (used by packaging)
    // This does a full scan + hash + writes manifest.json
    void generate_manifest(const std::string& engine_path, const std::string& output_path);

    // ── Legacy Scan (fallback if no manifest exists) ──
    void scan_engine_directory(const std::string& engine_path);
    void scan_directory(const std::string& dir_path);

    // Load/save the binary registry JSON (old format, kept for compat)
    void load_registry(const std::string& json_path);
    void save_registry(const std::string& json_path);

    // Get detected binaries
    const std::vector<BinaryInfo>& get_binaries() const { return binaries; }

    // Get/set default binary
    std::string get_default() const { return default_binary; }
    void set_default(const std::string& filepath);

    // Get info for the currently loaded binary
    const BinaryInfo* get_loaded_info() const;
    void set_loaded(const std::string& filepath);

    // Add a custom binary instance from a .so file
    // Copies the .so into engine/custom-NAME/<arch>/ and registers it
    bool add_custom_instance(const std::string& so_filepath, const std::string& name, const std::string& assets_dir);

    // Remove an instance by index from the in-memory list
    void remove_instance(int index) {
        if (index >= 0 && index < (int)binaries.size()) {
            binaries.erase(binaries.begin() + index);
        }
    }

    // Should we show the selector? (more than 1 binary found)
    bool should_show_selector() const { return binaries.size() > 1; }

    // Selection state for the boot GUI
    int selected_index = 0;

    // Compute SHA256 of a file
    static std::string compute_sha256(const std::string& filepath);

    // Get arch string for display
    static const char* arch_string(BinaryArch arch) {
        return (arch == BinaryArch::ARM64) ? "ARM64" : "ARM32";
    }

private:
    std::vector<BinaryInfo> binaries;
    std::string default_binary;
    std::string loaded_binary;

    // Known binary hashes (hardcoded fallbacks)
    static const std::map<std::string, std::pair<std::string, BinaryStatus>> KNOWN_HASHES;

    // Parse a version directory entry (used by scan + manifest generation)
    void scan_version_dir(const std::string& engine_path, const std::string& version_dir);
    void scan_arch_dir(const std::string& arch_path, const std::string& version_dir, BinaryArch arch);
    
    // JSON helpers
    static BinaryInfo parse_instance_json(const std::string& json_block);
    static std::string instance_to_json(const BinaryInfo& info, int indent = 4);
};
