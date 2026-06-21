#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdint.h>
#include <string>
#include <vector>
#include "loader/elf_loader.h"
#include "jni/jni_bridge.h"

class Emulator {
public:
    Emulator(uint8_t* guest_mem, uint32_t mem_size);
    ~Emulator();

    void set_pc(uint32_t pc);
    uint32_t get_pc();
    uint32_t get_lr();
    void set_reg(int reg, uint32_t value);
    uint32_t get_reg(int reg);
    float get_vfp_reg(int reg);

    void run(uint32_t start_pc);
    uint32_t call(uint32_t addr, const std::vector<uint32_t>& args);

    void set_bridge(JniBridge* bridge) { this->bridge = bridge; }
    void handle_bridge_call(uint32_t address);
    uint32_t get_bridge_base();
    uint8_t* get_memory_base() { return memory; }
    void* get_uc_handle() { return uc; }

    // Public for hooks
    JniBridge* bridge;
    void record_pc(uint32_t pc);
    void print_trace();
    bool quiet_mode = false;  // Suppress per-call logging

private:
    uint8_t* memory;
    uint32_t size;
    void* uc; // uc_engine*
    std::vector<uint32_t> last_pcs;
};


#endif
