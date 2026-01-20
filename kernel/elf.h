#ifndef ELF_H
#define ELF_H

#include <stdint.h>

// ELF Types
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;

// ELF File Header
#define EI_NIDENT 16

typedef struct {
    uint8_t     e_ident[EI_NIDENT]; // Magic number and other info
    Elf32_Half  e_type;             // Object file type (exec file, or library)
    Elf32_Half  e_machine;          // Architecture Type (ARM or x86)
    Elf32_Word  e_version;          // Object file version (normally 1)
    Elf32_Addr  e_entry;            // Entry point virtual address
    Elf32_Off   e_phoff;            // Program header table file offset
    Elf32_Off   e_shoff;            // Section header table file offset
    Elf32_Word  e_flags;            // Processor-specific flags
    Elf32_Half  e_ehsize;           // ELF header size in bytes
    Elf32_Half  e_phentsize;        // Size of one program header
    Elf32_Half  e_phnum;            // Number of program headers
    Elf32_Half  e_shentsize;        // Size of one section header
    Elf32_Half  e_shnum;            // Number of section headers
    Elf32_Half  e_shstrndx;         // Section header string table index
} __attribute__((packed)) Elf32_Ehdr;

// e_ident Indices
#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_PAD      7

// Magic Number Values
#define ELFMAG0     0x7F
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'

// Program Header
typedef struct {
    Elf32_Word  p_type;     // Segment type
    Elf32_Off   p_offset;   // Segment file offset
    Elf32_Addr  p_vaddr;    // Segment virtual address
    Elf32_Addr  p_paddr;    // Segment physical address
    Elf32_Word  p_filesz;   // Segment size in file
    Elf32_Word  p_memsz;    // Segment size in memory
    Elf32_Word  p_flags;    // Segment flags
    Elf32_Word  p_align;    // Segment alignment
} __attribute__((packed)) Elf32_Phdr;

// Segment Types (p_type)
#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_NOTE     4
#define PT_SHLIB    5
#define PT_PHDR     6

// e_type (Object file type)
#define ET_NONE     0
#define ET_REL      1
#define ET_EXEC     2
#define ET_DYN      3
#define ET_CORE     4

// e_machine (Architecture)
#define EM_NONE     0
#define EM_386      3       // Intel 80386

// Function Prototype
// Loads an ELF file from the file system and returns the entry point address.
// Returns 0 on failure.
uint32_t elf_load(char *filename);

#endif
