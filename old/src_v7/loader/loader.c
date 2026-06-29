#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

static so_module *g_head = NULL, *g_tail = NULL;

static int _so_load(so_module *mod, void *so_data) {
    mod->ehdr = (Elf32_Ehdr *)so_data;
    mod->phdr = (Elf32_Phdr *)((uintptr_t)so_data + mod->ehdr->e_phoff);
    mod->shdr = (Elf32_Shdr *)((uintptr_t)so_data + mod->ehdr->e_shoff);
    mod->shstr = (char *)((uintptr_t)so_data + mod->shdr[mod->ehdr->e_shstrndx].sh_offset);

    // Calculate total size needed
    size_t min_vaddr = 0xFFFFFFFF, max_vaddr = 0;
    for (int i = 0; i < mod->ehdr->e_phnum; i++) {
        if (mod->phdr[i].p_type == PT_LOAD) {
            if (mod->phdr[i].p_vaddr < min_vaddr) min_vaddr = mod->phdr[i].p_vaddr;
            if (mod->phdr[i].p_vaddr + mod->phdr[i].p_memsz > max_vaddr) 
                max_vaddr = mod->phdr[i].p_vaddr + mod->phdr[i].p_memsz;
        }
    }

    size_t total_size = ALIGN_MEM(max_vaddr - min_vaddr, 0x1000);
    // Allocate memory for the library. 
    // Ideally we want 32-bit addresses for ARM emulation.
    void* base = mmap(NULL, total_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    mod->text_base = base;
    mod->text_size = total_size;

    for (int i = 0; i < mod->ehdr->e_phnum; i++) {
        if (mod->phdr[i].p_type == PT_LOAD) {
            void* dest = (void*)((uintptr_t)base + (mod->phdr[i].p_vaddr - min_vaddr));
            memcpy(dest, (void*)((uintptr_t)so_data + mod->phdr[i].p_offset), mod->phdr[i].p_filesz);
            if (mod->phdr[i].p_memsz > mod->phdr[i].p_filesz) {
                memset((void*)((uintptr_t)dest + mod->phdr[i].p_filesz), 0, mod->phdr[i].p_memsz - mod->phdr[i].p_filesz);
            }
        }
    }

    // Extract section info
    for (int i = 0; i < mod->ehdr->e_shnum; i++) {
        char *sh_name = mod->shstr + mod->shdr[i].sh_name;
        uintptr_t sh_addr = (uintptr_t)base + (mod->shdr[i].sh_addr - min_vaddr);
        size_t sh_size = mod->shdr[i].sh_size;

        if (strcmp(sh_name, ".dynamic") == 0) {
            mod->dynamic = (Elf32_Dyn *)sh_addr;
            mod->num_dynamic = sh_size / sizeof(Elf32_Dyn);
        } else if (strcmp(sh_name, ".dynstr") == 0) {
            mod->dynstr = (char *)sh_addr;
        } else if (strcmp(sh_name, ".dynsym") == 0) {
            mod->dynsym = (Elf32_Sym *)sh_addr;
            mod->num_dynsym = sh_size / sizeof(Elf32_Sym);
        } else if (strcmp(sh_name, ".rel.dyn") == 0) {
            mod->reldyn = (Elf32_Rel *)sh_addr;
            mod->num_reldyn = sh_size / sizeof(Elf32_Rel);
        } else if (strcmp(sh_name, ".rel.plt") == 0) {
            mod->relplt = (Elf32_Rel *)sh_addr;
            mod->num_relplt = sh_size / sizeof(Elf32_Rel);
        } else if (strcmp(sh_name, ".init_array") == 0) {
            mod->init_array = (void (**)(void))sh_addr;
            mod->num_init_array = sh_size / sizeof(void *);
        } else if (strcmp(sh_name, ".hash") == 0) {
            mod->hash = (uint32_t *)sh_addr;
        }
    }

    if (g_tail) g_tail->next = mod;
    else g_head = mod;
    g_tail = mod;

    return 0;
}

int so_file_load(so_module *mod, const char *filename, uintptr_t load_addr) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return -1;

    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    void* data = malloc(size);
    read(fd, data, size);
    close(fd);

    int res = _so_load(mod, data);
    // free(data); // ehdr/phdr point into this, so we can't free it yet if we use them
    return res;
}

int so_relocate(so_module *mod) {
    uintptr_t base = (uintptr_t)mod->text_base;
    
    // Simplified relocation loop
    for (int i = 0; i < mod->num_reldyn + mod->num_relplt; i++) {
        Elf32_Rel *rel = i < mod->num_reldyn ? &mod->reldyn[i] : &mod->relplt[i - mod->num_reldyn];
        Elf32_Sym *sym = &mod->dynsym[ELF32_R_SYM(rel->r_info)];
        uintptr_t *ptr = (uintptr_t *)(base + rel->r_offset);

        int type = ELF32_R_TYPE(rel->r_info);
        switch (type) {
            case R_ARM_ABS32:
                if (sym->st_shndx != SHN_UNDEF)
                    *ptr += base + sym->st_value;
                break;
            case R_ARM_RELATIVE:
                *ptr += base;
                break;
            case R_ARM_GLOB_DAT:
            case R_ARM_JUMP_SLOT:
                if (sym->st_shndx != SHN_UNDEF)
                    *ptr = base + sym->st_value;
                break;
        }
    }
    return 0;
}

uintptr_t so_symbol(so_module *mod, const char *symbol) {
    for (int i = 0; i < mod->num_dynsym; i++) {
        if (strcmp(mod->dynstr + mod->dynsym[i].st_name, symbol) == 0) {
            return (uintptr_t)mod->text_base + mod->dynsym[i].st_value;
        }
    }
    return 0;
}

int so_resolve(so_module *mod, so_default_dynlib *default_dynlib, int size_default_dynlib) {
    uintptr_t base = (uintptr_t)mod->text_base;
    int num_libs = size_default_dynlib / sizeof(so_default_dynlib);

    for (int i = 0; i < mod->num_reldyn + mod->num_relplt; i++) {
        Elf32_Rel *rel = i < mod->num_reldyn ? &mod->reldyn[i] : &mod->relplt[i - mod->num_reldyn];
        Elf32_Sym *sym = &mod->dynsym[ELF32_R_SYM(rel->r_info)];
        uintptr_t *ptr = (uintptr_t *)(base + rel->r_offset);

        if (sym->st_shndx == SHN_UNDEF) {
            const char* name = mod->dynstr + sym->st_name;
            for (int j = 0; j < num_libs; j++) {
                if (strcmp(name, default_dynlib[j].symbol) == 0) {
                    *ptr = default_dynlib[j].func;
                    break;
                }
            }
        }
    }
    return 0;
}

void so_initialize(so_module *mod) {
    // In a real ARM host, we'd call the init functions.
    // Here we might need to emulated-call them.
    printf("Loader: Found %d init functions\n", mod->num_init_array);
}
