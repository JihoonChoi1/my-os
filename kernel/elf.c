#include "elf.h"
#include "../fs/simplefs.h"
#include "../mm/kheap.h"

// External printing functions
extern void print_string(char *str);
extern void print_hex(uint32_t n);
extern void memory_copy(char *source, char *dest, int nbytes);
extern void memset(char *dest, char val, int nbytes); // If not available, we need to implement or use loop

// Simple memset implementation if not available globally
void my_memset(char *dest, char val, int nbytes) {
    for (int i=0; i<nbytes; i++) {
        dest[i] = val;
    }
}

uint32_t elf_load(char *filename) {
    print_string("[ELF] Loading file: ");
    print_string(filename);
    print_string("\n");
    sfs_inode inode;
    if (!fs_find_file(filename, &inode)) {
        print_string("[ELF] Error: File not found.\n");
        return 0;
    }
    // Allocate buffer for the entire file
    // Ideally, we should read just the header first, but for simplicity
    // with our current FS driver (which reads whole files easily), we read it all.
    // WARNING: This assumes the file fits in the heap!
    
    char *file_buffer = (char*)kmalloc(inode.size);
    if (!file_buffer) {
        print_string("[ELF] Error: Out of memory for file buffer.\n");
        return 0;
    }

    // Read file
    fs_read_file(&inode, file_buffer);

    // 1. Validate ELF Header
    Elf32_Ehdr *ehdr = (Elf32_Ehdr*)file_buffer;

    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        
        print_string("[ELF] Error: Invalid ELF Magic.\n");
        kfree(file_buffer);
        return 0;
    }

    // Check for Executable and Architecture
    if (ehdr->e_type != ET_EXEC) { 
        print_string("[ELF] Warning: Not an executable file (ET_EXEC).\n");
    }

    if (ehdr->e_machine != EM_386) {
        print_string("[ELF] Error: Not an i386 file!\n");
        kfree(file_buffer);
        return 0;
    }

    // 2. Load Program Headers
    Elf32_Phdr *phdr = (Elf32_Phdr*)(file_buffer + ehdr->e_phoff);
    
    for (int i = 0; i < ehdr->e_phnum; i++) {
        // We only care about PT_LOAD segments
        if (phdr[i].p_type == PT_LOAD) {
            
            print_string("[ELF] Loading Segment at 0x");
            print_hex(phdr[i].p_vaddr);
            print_string(", File Size: ");
            print_hex(phdr[i].p_filesz);
            print_string(", Mem Size: ");
            print_hex(phdr[i].p_memsz);
            print_string("\n");

            // Destination in memory (Virtual Address)
            char *dest = (char*)phdr[i].p_vaddr;
            
            // Source in file buffer
            char *src = file_buffer + phdr[i].p_offset;

            // Copy file data to memory
            memory_copy(src, dest, phdr[i].p_filesz);

            // Zero out remaining memory (BSS section usually)
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                my_memset(dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    uint32_t entry_point = ehdr->e_entry;
    
    // Cleanup
    kfree(file_buffer);

    print_string("[ELF] Loaded successfully. Entry point: 0x");
    print_hex(entry_point);
    print_string("\n");

    return entry_point;
}
