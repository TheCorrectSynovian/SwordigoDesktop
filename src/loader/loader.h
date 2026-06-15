#ifndef LOADER_H
#define LOADER_H

#include <elf.h>
#include <stdint.h>
#include <stddef.h>

#define ALIGN_MEM(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define MAX_DATA_SEG 4

typedef struct so_module {
    struct so_module *next;

    void* text_base;
    size_t text_size;
    void* data_base[MAX_DATA_SEG];
    size_t data_size[MAX_DATA_SEG];
    int n_data;

    Elf32_Ehdr *ehdr;
    Elf32_Phdr *phdr;
    Elf32_Shdr *shdr;

    Elf32_Dyn *dynamic;
    Elf32_Sym *dynsym;
    Elf32_Rel *reldyn;
    Elf32_Rel *relplt;

    void (** init_array)(void);
    uint32_t *hash;

    int num_dynamic;
    int num_dynsym;
    int num_reldyn;
    int num_relplt;
    int num_init_array;

    char *soname;
    char *shstr;
    char *dynstr;
} so_module;

typedef struct {
    char *symbol;
    uintptr_t func;
} so_default_dynlib;

int so_file_load(so_module *mod, const char *filename, uintptr_t load_addr);
int so_relocate(so_module *mod);
int so_resolve(so_module *mod, so_default_dynlib *default_dynlib, int size_default_dynlib);
void so_initialize(so_module *mod);
uintptr_t so_symbol(so_module *mod, const char *symbol);

#endif
