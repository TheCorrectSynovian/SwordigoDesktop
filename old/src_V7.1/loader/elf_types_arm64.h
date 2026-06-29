#ifndef ELF_TYPES_ARM64_H
#define ELF_TYPES_ARM64_H

#include <stdint.h>

// ============================================================================
// 64-bit ELF type definitions for AArch64/ARM64
// Mirror of elf_types.h but for Elf64_* structures
// ============================================================================

typedef uint64_t Elf64_Addr;
typedef uint16_t Elf64_Half;
typedef uint64_t Elf64_Off;
typedef int32_t  Elf64_Sword;
typedef uint32_t Elf64_Word;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Xword;

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    Elf64_Word  p_type;
    Elf64_Word  p_flags;     // NOTE: flags moved before offset in Elf64!
    Elf64_Off   p_offset;
    Elf64_Addr  p_vaddr;
    Elf64_Addr  p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
} Elf64_Phdr;

typedef struct {
    Elf64_Word  sh_name;
    Elf64_Word  sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr  sh_addr;
    Elf64_Off   sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word  sh_link;
    Elf64_Word  sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} Elf64_Shdr;

typedef struct {
    Elf64_Sxword d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr  d_ptr;
    } d_un;
} Elf64_Dyn;

// NOTE: Elf64_Sym has a DIFFERENT field order than Elf32_Sym!
// st_info and st_other come BEFORE st_shndx, and st_value/st_size are 64-bit
typedef struct {
    Elf64_Word    st_name;
    unsigned char st_info;     // Moved up (was after st_size in Elf32)
    unsigned char st_other;    // Moved up
    Elf64_Half    st_shndx;    // Moved up
    Elf64_Addr    st_value;    // Now 64-bit
    Elf64_Xword   st_size;     // Now 64-bit
} Elf64_Sym;

// ARM64 uses RELA (with addend), NOT REL like ARM32
typedef struct {
    Elf64_Addr    r_offset;
    Elf64_Xword   r_info;
} Elf64_Rel;

typedef struct {
    Elf64_Addr    r_offset;
    Elf64_Xword   r_info;
    Elf64_Sxword  r_addend;    // ARM64 uses RELA (has explicit addend)
} Elf64_Rela;

#define ELF64_R_SYM(val)  ((val) >> 32)
#define ELF64_R_TYPE(val) ((val) & 0xffffffffUL)

// AArch64 relocation types
#define R_AARCH64_ABS64       257
#define R_AARCH64_GLOB_DAT    1025
#define R_AARCH64_JUMP_SLOT   1026
#define R_AARCH64_RELATIVE    1027

// Shared constants (same as 32-bit)
#ifndef PT_LOAD
#define PT_LOAD    1
#endif
#ifndef DT_SONAME
#define DT_SONAME  14
#define DT_NEEDED  1
#define SHN_UNDEF  0
#endif

// DT_RELA / DT_RELASZ for RELA relocations
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_JMPREL   23
#define DT_PLTRELSZ 2
#define DT_PLTREL   20
#define DT_RELSZ    18
#define DT_RELENT   19

#ifndef ELFMAG
#define ELFMAG "\177ELF"
#define SELFMAG 4
#endif

#ifndef PF_X
#define PF_X 1
#define PF_W 2
#define PF_R 4
#endif

// ELF class identifiers
#define ELFCLASS32 1
#define ELFCLASS64 2

// Machine types
#define EM_ARM     40
#define EM_AARCH64 183

#endif
