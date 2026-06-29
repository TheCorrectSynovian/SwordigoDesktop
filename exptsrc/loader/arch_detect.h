#ifndef ARCH_DETECT_H
#define ARCH_DETECT_H

#include <string>
#include <fstream>
#include <cstdint>

// Detect the CPU architecture of an ELF binary by reading its header.
// Returns "arm32", "arm64", or "unknown".
inline std::string detect_elf_arch(const std::string& filepath) {
    std::ifstream f(filepath, std::ios::binary);
    if (!f) return "unknown";

    // Read ELF magic (first 4 bytes)
    char magic[4];
    f.read(magic, 4);
    if (magic[0] != 0x7f || magic[1] != 'E' || magic[2] != 'L' || magic[3] != 'F') {
        return "unknown";  // Not an ELF file
    }

    // Byte 4 (EI_CLASS): 1 = 32-bit, 2 = 64-bit
    uint8_t ei_class;
    f.read((char*)&ei_class, 1);

    // Skip EI_DATA (byte 5) and EI_VERSION (byte 6-7) and padding
    // Jump to e_machine at offset 18 (32-bit) or 18 (64-bit) — same offset!
    f.seekg(18, std::ios::beg);
    uint16_t e_machine;
    f.read((char*)&e_machine, 2);

    // ARM: e_machine = 40 (EM_ARM)
    // AArch64: e_machine = 183 (EM_AARCH64)
    if (e_machine == 40 && ei_class == 1) return "arm32";
    if (e_machine == 183 && ei_class == 2) return "arm64";

    return "unknown";
}

// Quick check: is this an ARM64 binary?
inline bool is_arm64_binary(const std::string& filepath) {
    return detect_elf_arch(filepath) == "arm64";
}

// Also detect from the engine path convention: arm64-v8a in path
inline bool path_indicates_arm64(const std::string& filepath) {
    return filepath.find("arm64-v8a") != std::string::npos;
}

#endif
