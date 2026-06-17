#ifndef EMULATOR_ARM64_H
#define EMULATOR_ARM64_H

#include <stdint.h>
#include <string>
#include <vector>
#include "loader/elf_loader_arm64.h"
#include "jni/jni_bridge.h"

// ARM64 emulator — uses Unicorn in AArch64 mode
// Key differences from ARM32 Emulator:
//   - 64-bit registers (X0-X30, SP, PC)
//   - No Thumb mode (single instruction set)
//   - call() passes args in X0-X7 (8 regs!) instead of R0-R3
//   - Dedicated D0-D7 registers for float/double args
//   - Bridge trampoline uses 'ret' (0xD65F03C0) instead of 'bx lr' (0x4770)

class EmulatorArm64 {
public:
    EmulatorArm64(uint8_t* guest_mem, uint64_t mem_size);
    ~EmulatorArm64();

    void set_pc(uint64_t pc);
    uint64_t get_pc();
    uint64_t get_lr();
    
    // Integer registers X0-X30
    void set_reg(int reg, uint64_t value);
    uint64_t get_reg(int reg);
    
    // SIMD/FP registers D0-D31 (double) and S0-S31 (float)
    void set_dreg(int reg, double value);
    double get_dreg(int reg);
    void set_sreg(int reg, float value);
    float get_sreg(int reg);

    // Execute function at addr, returns X0
    void run(uint64_t start_pc);
    uint64_t call(uint64_t addr, const std::vector<uint64_t>& args);

    void set_bridge(JniBridge* bridge) { this->bridge = bridge; }
    void handle_bridge_call(uint64_t address);
    uint64_t get_bridge_base();
    uint8_t* get_memory_base() { return memory; }

    // Public for hooks
    JniBridge* bridge;
    void record_pc(uint64_t pc);
    void print_trace();
    bool quiet_mode = false;

private:
    uint8_t* memory;
    uint64_t size;
    void* uc; // uc_engine*
    std::vector<uint64_t> last_pcs;
};

#endif
