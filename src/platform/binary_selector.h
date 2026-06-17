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

// Info about a detected game binary
struct BinaryInfo {
    std::string filename;       // e.g. "libswordigo.so"
    std::string sha256;         // SHA256 hex string
    std::string version;        // e.g. "1.4.6"
    std::string label;          // e.g. "v1.4.6 (Stable)"
    BinaryStatus status;
    size_t file_size;           // bytes
    bool is_default;
};

// Manages binary detection, hashing, and selection
class BinarySelector {
public:
    BinarySelector();

    // Scan a directory for libswordigo*.so files
    void scan_directory(const std::string& dir_path);

    // Load/save the binary registry JSON
    void load_registry(const std::string& json_path);
    void save_registry(const std::string& json_path);

    // Get detected binaries
    const std::vector<BinaryInfo>& get_binaries() const { return binaries; }

    // Get/set default binary
    std::string get_default() const { return default_binary; }
    void set_default(const std::string& filename);

    // Get info for the currently loaded binary
    const BinaryInfo* get_loaded_info() const;
    void set_loaded(const std::string& filename);

    // Compute SHA256 of a file
    static std::string compute_sha256(const std::string& filepath);

    // Should we show the selector? (more than 1 binary found)
    bool should_show_selector() const { return binaries.size() > 1; }

    // Selection state for the boot GUI
    int selected_index = 0;

private:
    std::vector<BinaryInfo> binaries;
    std::string default_binary;
    std::string loaded_binary;

    // Known binary hashes (hardcoded fallbacks)
    static const std::map<std::string, std::pair<std::string, BinaryStatus>> KNOWN_HASHES;
};
