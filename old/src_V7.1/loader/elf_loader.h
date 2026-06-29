#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>
#include <vector>
#include <string>
#include <map>

// Simplified ELF structures for 32-bit ARM
#include "elf_types.h"

class JniBridge;

struct so_module {
    uintptr_t base_addr;
    size_t mem_size;
    
    uintptr_t text_base;
    size_t text_size;
    
    std::vector<uintptr_t> data_bases;
    std::vector<size_t> data_sizes;

    Elf32_Ehdr *ehdr;
    Elf32_Phdr *phdr;
    Elf32_Shdr *shdr;

    Elf32_Dyn *dynamic;
    Elf32_Sym *dynsym;
    Elf32_Rel *reldyn;
    Elf32_Rel *relplt;

    uint32_t *hash;
    char *dynstr;

    int num_dynsym;
    int num_reldyn;
    int num_relplt;
    
    uint32_t init_array_vaddr = 0;
    uint32_t init_array_size = 0;
    
    std::string soname;
};

struct so_default_dynlib {
    std::string symbol;
    uintptr_t func; // This will be the "magic address" for the trampoline
};

class ElfLoader {
public:
    ElfLoader(uint8_t* guest_mem_base, uint32_t guest_mem_size);
    
    int load(so_module* mod, const std::string& filename, uint32_t load_addr);
    int relocate(so_module* mod);
    int resolve(so_module* mod, const std::vector<so_default_dynlib>& imports);
    int resolve_all_to_bridge(so_module* mod, JniBridge* bridge, uint32_t globals_base = 0);
    
    uint32_t get_symbol_vaddr(so_module* mod, const std::string& name);

private:
    uint8_t* guest_base;
    uint32_t guest_limit;
    
    uint32_t so_hash(const uint8_t *name);
    int find_symbol_index(so_module *mod, const char *symbol);
};

#endif
