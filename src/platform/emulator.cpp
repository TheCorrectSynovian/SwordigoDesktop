#include "emulator.h"
#include <iostream>
#include <cstring>
#include <unicorn/unicorn.h>

// Forward declarations from game/camera_override.cpp
extern uint32_t g_cam_ctrl_ptr;
extern void cam_capture_controller(uint32_t this_ptr);

static uint32_t g_instruction_count = 0;

static bool hook_mem_unmapped(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    std::cerr << "[FAULT] Memory UNKNOWN unmapped at 0x" << std::hex << address << " (size: " << size << ") @instr " << std::dec << g_instruction_count << std::endl;
    uint32_t pc, lr, sp;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    std::cerr << "  PC=0x" << std::hex << pc << " LR=0x" << lr << " SP=0x" << sp << std::dec << std::endl;
    return false;
}

// Lightweight global code hook — only counts instructions in debug mode
static void hook_code_count(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    g_instruction_count++;
}

// Bridge-specific hook — only fires for bridge address range (0xFF000000+)
static void hook_bridge(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    Emulator* emu = (Emulator*)user_data;
    emu->handle_bridge_call((uint32_t)address);
}

// Camera capture hook — only fires at the specific CameraController constructor address
static void hook_camera_ctor(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    if (g_cam_ctrl_ptr == 0) {
        uint32_t this_ptr = 0;
        uc_reg_read(uc, UC_ARM_REG_R0, &this_ptr);
        if (this_ptr != 0) {
            cam_capture_controller(this_ptr);
        }
    }
}


Emulator::Emulator(uint8_t* guest_mem, uint32_t mem_size) 
    : memory(guest_mem), size(mem_size), bridge(nullptr), uc(nullptr) {
    
    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, (uc_engine**)&uc);
    if (err) {
        std::cerr << "Failed on uc_open() with error returned: " << err << std::endl;
        return;
    }

    err = uc_mem_map_ptr((uc_engine*)uc, 0, size, UC_PROT_ALL, memory);
    if (err) {
        std::cerr << "Failed on uc_mem_map_ptr() with error returned: " << err << std::endl;
        return;
    }
    
    err = uc_mem_map((uc_engine*)uc, 0xFF000000, 0x1000000, UC_PROT_ALL);
    if (err) {
        std::cerr << "Failed on uc_mem_map() bridge with error: " << err << std::endl;
    }

    // Fill bridge with 'bx lr' in Thumb (0x4770)
    uint16_t bx_lr = 0x4770;
    for (uint32_t addr = 0xFF000000; addr < 0xFF100000; addr += 2) {
        uc_mem_write((uc_engine*)uc, addr, &bx_lr, 2);
    }

    // Enable VFP
    uint32_t cp15;
    uc_reg_read((uc_engine*)uc, UC_ARM_REG_C1_C0_2, &cp15);
    cp15 |= (0xf << 20);
    uc_reg_write((uc_engine*)uc, UC_ARM_REG_C1_C0_2, &cp15);
    uint32_t fpexc = 0x40000000;
    uc_reg_write((uc_engine*)uc, UC_ARM_REG_FPEXC, &fpexc);

    uint32_t stack_base = size - 0x1000; 
    set_reg(13, stack_base); 

    err = uc_mem_map((uc_engine*)uc, 0xE0000000, 0x1000, UC_PROT_READ | UC_PROT_EXEC);
    if (err) {
        std::cerr << "Failed to map magic LR page: " << err << std::endl;
    }

    // Hook 1: Bridge calls — only for bridge address range (HUGE perf win vs global hook)
    uc_hook bridge_hook;
    uc_hook_add((uc_engine*)uc, &bridge_hook, UC_HOOK_CODE, (void*)hook_bridge, this,
                0xFF000000, 0xFF100000);

    // Hook 2: Camera controller capture — only fires at the constructor address
    uc_hook cam_hook;
    uc_hook_add((uc_engine*)uc, &cam_hook, UC_HOOK_CODE, (void*)hook_camera_ctor, this,
                0x002e35c4, 0x002e35c6);

    // Hook 3: Unmapped memory
    uc_hook mem_hook;
    uc_hook_add((uc_engine*)uc, &mem_hook, UC_HOOK_MEM_UNMAPPED, (void*)hook_mem_unmapped, this, 1, 0);
}

Emulator::~Emulator() {
    if (uc) uc_close((uc_engine*)uc);
}

void Emulator::set_pc(uint32_t pc) {
    uc_reg_write((uc_engine*)uc, UC_ARM_REG_PC, &pc);
}

void Emulator::set_reg(int reg, uint32_t value) {
    int uc_reg = UC_ARM_REG_R0 + reg;
    if (reg == 13) uc_reg = UC_ARM_REG_SP;
    if (reg == 14) uc_reg = UC_ARM_REG_LR;
    if (reg == 15) uc_reg = UC_ARM_REG_PC;
    uc_reg_write((uc_engine*)uc, uc_reg, &value);
}

uint32_t Emulator::get_reg(int reg) {
    uint32_t val;
    int uc_reg = UC_ARM_REG_R0 + reg;
    if (reg == 13) uc_reg = UC_ARM_REG_SP;
    if (reg == 14) uc_reg = UC_ARM_REG_LR;
    if (reg == 15) uc_reg = UC_ARM_REG_PC;
    uc_reg_read((uc_engine*)uc, uc_reg, &val);
    return val;
}

float Emulator::get_vfp_reg(int reg) {
    union {
        uint32_t i;
        float f;
    } val;
    int uc_reg = UC_ARM_REG_S0 + reg;
    uc_reg_read((uc_engine*)uc, uc_reg, &val.i);
    return val.f;
}

uint32_t Emulator::get_pc() {
    uint32_t val;
    uc_reg_read((uc_engine*)uc, UC_ARM_REG_PC, &val);
    return val;
}

uint32_t Emulator::get_lr() {
    uint32_t val;
    uc_reg_read((uc_engine*)uc, UC_ARM_REG_LR, &val);
    return val;
}

uint32_t Emulator::get_bridge_base() { return 0xFF000000; }

void Emulator::run(uint32_t start_pc) {
    uint32_t magic_lr = 0xE0000000;
    set_reg(14, magic_lr);
    
    uint32_t pc = start_pc;
    uc_reg_write((uc_engine*)uc, UC_ARM_REG_PC, &pc);
    
    uint32_t cpsr;
    uc_reg_read((uc_engine*)uc, UC_ARM_REG_CPSR, &cpsr);
    if (start_pc & 1) {
        cpsr |= (1 << 5); 
    } else {
        cpsr &= ~(1 << 5);
    }
    uc_reg_write((uc_engine*)uc, UC_ARM_REG_CPSR, &cpsr);

    uc_err err = uc_emu_start((uc_engine*)uc, (uint64_t)pc, (uint64_t)magic_lr, 0, 0);
    if (err) {
        std::cerr << "Unicorn emulation error: " << uc_strerror(err) << std::endl;
        uint32_t curr_pc;
        uc_reg_read((uc_engine*)uc, UC_ARM_REG_PC, &curr_pc);
        std::cerr << "Failed at PC: 0x" << std::hex << curr_pc << std::dec << std::endl;
        print_trace();
    }
    
    if (!quiet_mode) {
        std::cout << "[Emulator] Function executed " << g_instruction_count << " instructions" << std::endl;
    }
    g_instruction_count = 0;
}

uint32_t Emulator::call(uint32_t addr, const std::vector<uint32_t>& args) {
    if (!quiet_mode) {
        std::cout << "[Emulator] Calling guest function at 0x" << std::hex << addr << std::dec << std::endl;
    }
    for (int i = 0; i < args.size() && i < 4; ++i) {
        set_reg(i, args[i]);
    }

    run(addr);
    uint32_t result = get_reg(0);
    if (!quiet_mode) {
        std::cout << "[Emulator] Function returned with R0=0x" << std::hex << result << std::dec << std::endl;
    }
    return result;
}

void Emulator::handle_bridge_call(uint32_t address) {
    if (bridge) {
        uint32_t lr = get_reg(14);
        bridge->call_handler(address, this);
        
        uint32_t cpsr;
        uc_reg_read((uc_engine*)uc, UC_ARM_REG_CPSR, &cpsr);
        if (lr & 1) {
            cpsr |= (1 << 5); // Thumb
        } else {
            cpsr &= ~(1 << 5); // ARM
        }
        uc_reg_write((uc_engine*)uc, UC_ARM_REG_CPSR, &cpsr);

        set_pc(lr);
    }
}

void Emulator::record_pc(uint32_t pc) {
    if (pc >= 0x1000000 && pc < 0x1500000) {
        last_pcs.push_back(pc);
        if (last_pcs.size() > 50) {
            last_pcs.erase(last_pcs.begin());
        }
    }
}

void Emulator::print_trace() {
    std::cerr << "--- PC Execution Trace (Last " << last_pcs.size() << " instructions) ---" << std::endl;
    for (size_t i = 0; i < last_pcs.size(); ++i) {
        std::cerr << "  #" << i << ": 0x" << std::hex << last_pcs[i] << std::dec << std::endl;
    }
    std::cerr << "---------------------------------------------------" << std::endl;
}
