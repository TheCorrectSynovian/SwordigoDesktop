#ifndef EMULATOR_ARM64_H
#define EMULATOR_ARM64_H

#include <stdint.h>
#include <string>
#include <vector>
#include <iostream>
#include "loader/elf_loader_arm64.h"
#include "jni/jni_bridge_arm64.h"

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

    void set_bridge(JniBridge64* bridge) { this->bridge = bridge; }
    void handle_bridge_call(uint64_t address);
    uint64_t get_bridge_base();
    uint8_t* get_memory_base() { return memory; }
    void* get_uc_handle() { return uc; }

    // Public for hooks
    JniBridge64* bridge;
    void record_pc(uint64_t pc);
    void print_trace();
    void set_lr(uint64_t lr);
    bool quiet_mode = false;
    
    // Bridge redirect: if set by a handler, handle_bridge_call uses this PC
    // instead of the saved LR. Reset to 0 after use.
    uint64_t redirect_pc = 0;

    // Deferred thread execution queue — avoids nested uc_emu_start
    struct DeferredThread {
        uint64_t start_routine;
        uint64_t arg;
    };
    std::vector<DeferredThread> pending_threads;
    
    // Queue a thread for deferred execution (called from pthread_create bridge)
    void queue_thread(uint64_t start_routine, uint64_t arg) {
        pending_threads.push_back({start_routine, arg});
        std::cerr << "[Thread64] Queued deferred thread func=0x" << std::hex 
                  << start_routine << " arg=0x" << arg << std::dec << std::endl;
    }
    
    // Run all pending threads (call from game loop between frames)
    void run_pending_threads() {
        while (!pending_threads.empty()) {
            DeferredThread t = pending_threads.front();
            pending_threads.erase(pending_threads.begin());
            std::cout << "[Thread64] Running deferred thread func=0x" << std::hex 
                      << t.start_routine << " arg=0x" << t.arg << std::dec << std::endl;
            call(t.start_routine, {t.arg});
            std::cout << "[Thread64] Deferred thread completed." << std::endl;
        }
    }
    
    bool has_pending_threads() const { return !pending_threads.empty(); }
    
    // Change Unicorn memory protection for a region
    void protect_memory(uint64_t addr, uint64_t size, uint32_t perms);

private:
    uint8_t* memory;
    uint64_t size;
    void* uc; // uc_engine*
    std::vector<uint64_t> last_pcs;
};

#endif
