#include "platform/binary_selector.h"
#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <iomanip>
#include <algorithm>

// ============================================================================
// Minimal SHA-256 implementation (no OpenSSL dependency)
// ============================================================================

static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | (uint32_t)block[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=state[0], b=state[1], c=state[2], d=state[3];
    uint32_t e=state[4], f=state[5], g=state[6], h=state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + SHA256_K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

std::string BinarySelector::compute_sha256(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint8_t block[64];
    uint64_t total_bits = 0;

    while (file) {
        file.read((char*)block, 64);
        size_t bytes_read = file.gcount();
        total_bits += bytes_read * 8;

        if (bytes_read == 64) {
            sha256_transform(state, block);
        } else {
            // Padding
            block[bytes_read] = 0x80;
            for (size_t i = bytes_read + 1; i < 64; i++) block[i] = 0;

            if (bytes_read >= 56) {
                sha256_transform(state, block);
                memset(block, 0, 64);
            }

            // Length in bits (big-endian)
            for (int i = 7; i >= 0; i--)
                block[56 + (7 - i)] = (uint8_t)(total_bits >> (i * 8));

            sha256_transform(state, block);
        }
    }

    std::ostringstream oss;
    for (int i = 0; i < 8; i++)
        oss << std::hex << std::setw(8) << std::setfill('0') << state[i];
    return oss.str();
}

// ============================================================================
// Known binary hashes (hardcoded fallbacks)
// ============================================================================

const std::map<std::string, std::pair<std::string, BinaryStatus>> BinarySelector::KNOWN_HASHES = {
    {"cee15dd2730746269ce5db97d150371ebbad1f41371c6a728f1bb7d045632138",
        {"1.4.6", BinaryStatus::TESTED}},
    {"08d49dd6f7f8639a4c59f290ff2bb79254accf710f530bf53c2fce1659191c9e",
        {"1.4.12", BinaryStatus::TESTING}},
};

// ============================================================================
// Constructor
// ============================================================================

BinarySelector::BinarySelector() : default_binary("libswordigo.so") {}

// ============================================================================
// Scan directory for libswordigo*.so files
// ============================================================================

void BinarySelector::scan_directory(const std::string& dir_path) {
    binaries.clear();

    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        std::cerr << "[BinSel] Cannot open directory: " << dir_path << std::endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        // Match libswordigo*.so (but not .so.1 etc.)
        if (name.find("libswordigo") == 0 && name.length() > 3 &&
            name.substr(name.length() - 3) == ".so") {

            std::string full_path = dir_path;
            if (!full_path.empty() && full_path.back() != '/') full_path += "/";
            full_path += name;

            // Skip arm64 binary (we only support arm32)
            if (name.find("64") != std::string::npos) continue;
            // Skip patched binaries unless they're explicitly named
            // Actually include everything — let the user decide

            struct stat st;
            if (stat(full_path.c_str(), &st) != 0) continue;

            BinaryInfo info;
            info.filename = name;
            info.file_size = st.st_size;
            info.is_default = (name == default_binary);

            // Compute hash
            std::cout << "[BinSel] Hashing " << name << "..." << std::flush;
            info.sha256 = compute_sha256(full_path);
            std::cout << " " << info.sha256.substr(0, 16) << "..." << std::endl;

            // Look up in known hashes
            auto it = KNOWN_HASHES.find(info.sha256);
            if (it != KNOWN_HASHES.end()) {
                info.version = it->second.first;
                info.status = it->second.second;
            } else {
                info.version = "Unknown";
                info.status = BinaryStatus::UNKNOWN;
            }

            // Build label
            const char* status_str = (info.status == BinaryStatus::TESTED) ? "Stable" :
                                     (info.status == BinaryStatus::TESTING) ? "Testing" : "Unknown";
            info.label = "v" + info.version + " (" + status_str + ")";

            binaries.push_back(info);
        }
    }
    closedir(dir);

    // Sort: default first, then tested, then testing, then unknown
    std::sort(binaries.begin(), binaries.end(), [](const BinaryInfo& a, const BinaryInfo& b) {
        if (a.is_default != b.is_default) return a.is_default;
        if (a.status != b.status) return (int)a.status < (int)b.status;
        return a.filename < b.filename;
    });

    std::cout << "[BinSel] Found " << binaries.size() << " game binaries" << std::endl;
}

// ============================================================================
// JSON Registry — Simple hand-written parser (no external JSON lib needed)
// ============================================================================

void BinarySelector::load_registry(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f) {
        std::cout << "[BinSel] No registry found at " << json_path << " (will create on save)" << std::endl;
        return;
    }

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // Simple parsing: find "default" value
    auto find_value = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return "";
        pos = content.find("\"", pos + search.length() + 1);
        if (pos == std::string::npos) return "";
        size_t end = content.find("\"", pos + 1);
        if (end == std::string::npos) return "";
        return content.substr(pos + 1, end - pos - 1);
    };

    std::string def = find_value("default");
    if (!def.empty()) {
        default_binary = def;
        std::cout << "[BinSel] Default binary: " << default_binary << std::endl;
    }

    // Update is_default on binaries
    for (auto& b : binaries) {
        b.is_default = (b.filename == default_binary);
    }
}

void BinarySelector::save_registry(const std::string& json_path) {
    std::ofstream f(json_path);
    if (!f) {
        std::cerr << "[BinSel] Cannot write registry to " << json_path << std::endl;
        return;
    }

    f << "{\n";
    f << "  \"default\": \"" << default_binary << "\",\n";
    f << "  \"known_binaries\": {\n";
    for (size_t i = 0; i < binaries.size(); i++) {
        const auto& b = binaries[i];
        const char* status_str = (b.status == BinaryStatus::TESTED) ? "tested" :
                                 (b.status == BinaryStatus::TESTING) ? "testing" : "unknown";
        f << "    \"" << b.sha256 << "\": {\n";
        f << "      \"filename\": \"" << b.filename << "\",\n";
        f << "      \"version\": \"" << b.version << "\",\n";
        f << "      \"status\": \"" << status_str << "\",\n";
        f << "      \"label\": \"" << b.label << "\"\n";
        f << "    }" << (i + 1 < binaries.size() ? "," : "") << "\n";
    }
    f << "  }\n";
    f << "}\n";

    std::cout << "[BinSel] Registry saved to " << json_path << std::endl;
}

// ============================================================================
// Selection helpers
// ============================================================================

void BinarySelector::set_default(const std::string& filename) {
    default_binary = filename;
    for (auto& b : binaries) {
        b.is_default = (b.filename == filename);
    }
}

const BinaryInfo* BinarySelector::get_loaded_info() const {
    for (const auto& b : binaries) {
        if (b.filename == loaded_binary) return &b;
    }
    return nullptr;
}

void BinarySelector::set_loaded(const std::string& filename) {
    loaded_binary = filename;
}
