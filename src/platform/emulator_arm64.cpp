#include "emulator_arm64.h"
#include <iostream>
#include <cstring>
#include <unicorn/unicorn.h>

// ============================================================================
// ARM64 Emulator — AArch64 mode via Unicorn Engine
//
// Differences from ARM32:
//   - UC_ARCH_ARM64 / UC_MODE_ARM (no Thumb!)
//   - Registers: UC_ARM64_REG_X0..X30, SP, PC, LR
//   - Bridge trampoline: 'ret' = 0xD65F03C0 (4 bytes) vs 'bx lr' = 0x4770
//   - No CPSR Thumb bit manipulation
//   - VFP/NEON enabled by default on AArch64 (no CP15 setup needed)
//   - call() passes up to 8 integer args in X0-X7
// ============================================================================

// Forward declarations from game/camera_override.cpp
extern uint32_t g_cam_ctrl_ptr;
extern void cam_capture_controller(uint32_t this_ptr);

static uint64_t g_instruction_count_64 = 0;

// --- Unicorn hooks ---

static bool hook_mem_unmapped_64(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    std::cerr << "[FAULT/ARM64] Memory unmapped at 0x" << std::hex << address << " (size: " << size << ")" << std::dec << std::endl;
    uint64_t pc, lr, sp;
    uc_reg_read(uc, UC_ARM64_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM64_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM64_REG_SP, &sp);
    std::cerr << "  PC=0x" << std::hex << pc << " LR=0x" << lr << " SP=0x" << sp << std::dec << std::endl;
    return false;
}

static void hook_bridge_64(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    EmulatorArm64* emu = (EmulatorArm64*)user_data;
    emu->handle_bridge_call(address);
}

// TODO: Camera hook address for ARM64 binary — needs to be found via symbol
// static void hook_camera_ctor_64(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) { ... }

// --- Constructor ---

EmulatorArm64::EmulatorArm64(uint8_t* guest_mem, uint64_t mem_size)
    : memory(guest_mem), size(mem_size), bridge(nullptr), uc(nullptr) {
    
    // Open Unicorn in AArch64 mode (no Thumb, no mode switching!)
    uc_err err = uc_open(UC_ARCH_ARM64, UC_MODE_ARM, (uc_engine**)&uc);
    if (err) {
        std::cerr << "[ARM64] Failed on uc_open(): " << uc_strerror(err) << std::endl;
        return;
    }

    // Map guest memory (same flat mapping as ARM32)
    err = uc_mem_map_ptr((uc_engine*)uc, 0, size, UC_PROT_ALL, memory);
    if (err) {
        std::cerr << "[ARM64] Failed on uc_mem_map_ptr(): " << uc_strerror(err) << std::endl;
        return;
    }
    
    // Map bridge trampoline region (same range as ARM32: 0xFF000000)
    err = uc_mem_map((uc_engine*)uc, 0xFF000000, 0x1000000, UC_PROT_ALL);
    if (err) {
        std::cerr << "[ARM64] Failed to map bridge region: " << uc_strerror(err) << std::endl;
    }

    // Fill bridge region with 'ret' instructions (ARM64: 0xD65F03C0)
    // Unlike ARM32's 2-byte 'bx lr', ARM64 'ret' is 4 bytes, aligned to 4
    uint32_t ret_insn = 0xD65F03C0;
    for (uint64_t addr = 0xFF000000; addr < 0xFF100000; addr += 4) {
        uc_mem_write((uc_engine*)uc, addr, &ret_insn, 4);
    }

    // No VFP/CP15 setup needed — AArch64 has NEON/FP enabled by default!

    // Set stack pointer (same strategy as ARM32)
    uint64_t stack_base = (uint64_t)(size - 0x1000);
    uc_reg_write((uc_engine*)uc, UC_ARM64_REG_SP, &stack_base);

    // Map magic LR page (return detection)
    err = uc_mem_map((uc_engine*)uc, 0xE0000000, 0x1000, UC_PROT_READ | UC_PROT_EXEC);
    if (err) {
        std::cerr << "[ARM64] Failed to map magic LR page: " << uc_strerror(err) << std::endl;
    }
    // Write a 'ret' at magic LR so Unicorn stops cleanly
    uc_mem_write((uc_engine*)uc, 0xE0000000, &ret_insn, 4);

    // Hook 1: Bridge calls (0xFF000000 range)
    uc_hook bridge_hook;
    uc_hook_add((uc_engine*)uc, &bridge_hook, UC_HOOK_CODE, (void*)hook_bridge_64, this,
                0xFF000000, 0xFF100000);

    // Hook 2: Unmapped memory
    uc_hook mem_hook;
    uc_hook_add((uc_engine*)uc, &mem_hook, UC_HOOK_MEM_UNMAPPED, (void*)hook_mem_unmapped_64, this, 1, 0);
    
    // TODO: Hook for camera controller capture — address needs to come from ARM64 symbol table
    // (ARM32 uses hardcoded 0x002e35c4 — this WILL be different in ARM64 binary)

    std::cout << "[ARM64] Emulator initialized, stack at 0x" << std::hex << stack_base << std::dec << std::endl;
}

EmulatorArm64::~EmulatorArm64() {
    if (uc) uc_close((uc_engine*)uc);
}

// --- Register access ---

void EmulatorArm64::set_pc(uint64_t pc) {
    uc_reg_write((uc_engine*)uc, UC_ARM64_REG_PC, &pc);
}

uint64_t EmulatorArm64::get_pc() {
    uint64_t val;
    uc_reg_read((uc_engine*)uc, UC_ARM64_REG_PC, &val);
    return val;
}

uint64_t EmulatorArm64::get_lr() {
    uint64_t val;
    uc_reg_read((uc_engine*)uc, UC_ARM64_REG_LR, &val);
    return val;
}

void EmulatorArm64::set_reg(int reg, uint64_t value) {
    // X0-X28 map directly, X29=FP, X30=LR
    int uc_reg;
    if (reg <= 28)      uc_reg = UC_ARM64_REG_X0 + reg;
    else if (reg == 29) uc_reg = UC_ARM64_REG_FP;
    else if (reg == 30) uc_reg = UC_ARM64_REG_LR;
    else                uc_reg = UC_ARM64_REG_SP;
    uc_reg_write((uc_engine*)uc, uc_reg, &value);
}

uint64_t EmulatorArm64::get_reg(int reg) {
    uint64_t val;
    int uc_reg;
    if (reg <= 28)      uc_reg = UC_ARM64_REG_X0 + reg;
    else if (reg == 29) uc_reg = UC_ARM64_REG_FP;
    else if (reg == 30) uc_reg = UC_ARM64_REG_LR;
    else                uc_reg = UC_ARM64_REG_SP;
    uc_reg_read((uc_engine*)uc, uc_reg, &val);
    return val;
}

void EmulatorArm64::set_dreg(int reg, double value) {
    // D0-D31 for double-precision floats
    int uc_reg = UC_ARM64_REG_D0 + reg;
    uc_reg_write((uc_engine*)uc, uc_reg, &value);
}

double EmulatorArm64::get_dreg(int reg) {
    double val;
    int uc_reg = UC_ARM64_REG_D0 + reg;
    uc_reg_read((uc_engine*)uc, uc_reg, &val);
    return val;
}

void EmulatorArm64::set_sreg(int reg, float value) {
    // S0-S31 for single-precision floats
    int uc_reg = UC_ARM64_REG_S0 + reg;
    uc_reg_write((uc_engine*)uc, uc_reg, &value);
}

float EmulatorArm64::get_sreg(int reg) {
    float val;
    int uc_reg = UC_ARM64_REG_S0 + reg;
    uc_reg_read((uc_engine*)uc, uc_reg, &val);
    return val;
}

uint64_t EmulatorArm64::get_bridge_base() { return 0xFF000000; }

// --- Execution ---

void EmulatorArm64::run(uint64_t start_pc) {
    uint64_t magic_lr = 0xE0000000;
    uc_reg_write((uc_engine*)uc, UC_ARM64_REG_LR, &magic_lr);
    uc_reg_write((uc_engine*)uc, UC_ARM64_REG_PC, &start_pc);
    
    // No Thumb bit to set! AArch64 is always A64 mode.
    
    uc_err err = uc_emu_start((uc_engine*)uc, start_pc, magic_lr, 0, 0);
    if (err) {
        std::cerr << "[ARM64] Emulation error: " << uc_strerror(err) << std::endl;
        uint64_t curr_pc;
        uc_reg_read((uc_engine*)uc, UC_ARM64_REG_PC, &curr_pc);
        std::cerr << "  Failed at PC: 0x" << std::hex << curr_pc << std::dec << std::endl;
        print_trace();
    }
    
    if (!quiet_mode) {
        std::cout << "[ARM64] Function executed " << g_instruction_count_64 << " instructions" << std::endl;
    }
    g_instruction_count_64 = 0;
}

uint64_t EmulatorArm64::call(uint64_t addr, const std::vector<uint64_t>& args) {
    if (!quiet_mode) {
        std::cout << "[ARM64] Calling guest function at 0x" << std::hex << addr << std::dec << std::endl;
    }
    
    // ARM64 AAPCS: first 8 integer args go in X0-X7
    // (ARM32 only had R0-R3 = 4 registers, rest on stack)
    for (size_t i = 0; i < args.size() && i < 8; ++i) {
        set_reg(i, args[i]);
    }
    
    // TODO: if args.size() > 8, push remaining to stack (rare for JNI calls)

    run(addr);
    uint64_t result = get_reg(0); // Return in X0
    if (!quiet_mode) {
        std::cout << "[ARM64] Function returned with X0=0x" << std::hex << result << std::dec << std::endl;
    }
    return result;
}

// --- Bridge handling ---

void EmulatorArm64::handle_bridge_call(uint64_t address) {
    if (bridge) {
        uint64_t lr = get_lr();
        // In ARM64, bridge->call_handler needs to handle 64-bit addresses
        // For now, cast down to 32-bit for compatibility with existing JniBridge
        bridge->call_handler((uint32_t)address, (Emulator*)nullptr);
        // TODO: Need JniBridge arm64 variant that takes EmulatorArm64*
        
        // No Thumb bit manipulation needed — AArch64 is always A64
        set_pc(lr);
    }
}

// --- Debugging ---

void EmulatorArm64::record_pc(uint64_t pc) {
    if (pc >= 0x1000000 && pc < 0x2000000) {
        last_pcs.push_back(pc);
        if (last_pcs.size() > 50) {
            last_pcs.erase(last_pcs.begin());
        }
    }
}

void EmulatorArm64::print_trace() {
    std::cerr << "--- ARM64 PC Trace (Last " << last_pcs.size() << " instructions) ---" << std::endl;
    for (size_t i = 0; i < last_pcs.size(); ++i) {
        std::cerr << "  #" << i << ": 0x" << std::hex << last_pcs[i] << std::dec << std::endl;
    }
    std::cerr << "---------------------------------------------" << std::endl;
}
