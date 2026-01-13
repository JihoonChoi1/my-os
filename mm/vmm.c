#include "vmm.h"
#include "pmm.h"

// External helper for printing
extern void print_string(char* str);

// The Kernel's Page Directory
page_directory* kernel_directory = 0;

void vmm_init() {
    // Allocate a Page Directory from PMM
    // Since paging is OFF, the physical address returned is usable as a pointer.
    kernel_directory = (page_directory*)pmm_alloc_block();
    if (!kernel_directory) {
        print_string("VMM Error: Failed to allocate Page Directory!\n");
        return;
    }

    // Clear 1024 entries (Mark all as not present)
    for (int i = 0; i < TABLES_PER_DIRECTORY; i++) {
        // Attribute: Read/Write, Supervisor
        kernel_directory->m_entries[i] = 0 | I86_PTE_WRITABLE; 
    }

    // Allocate the First Page Table (To map 0MB - 4MB)
    page_table* first_table = (page_table*)pmm_alloc_block();
    if (!first_table) {
        print_string("VMM Error: Failed to allocate Page Table!\n");
        return;
    }

    // Identity Map the first 4MB
    // 0x00000000 -> 0x00000000
    // 0x00001000 -> 0x00001000
    // ...
    // This ensures that when we turn on paging, the currently running kernel code
    // (which is at 0x100000) keeps running without crashing.
    for (int i = 0; i < PAGES_PER_TABLE; i++) {
        uint32_t frame = i * PAGE_SIZE; // 0, 4096, 8192 ...
        
        // Entry = Frame Address | Present | Writable | Supervisor (Kernel Only)
        // User mode bit (I86_PTE_USER) is NOT set, so user code cannot access this.
        first_table->m_entries[i] = frame | I86_PTE_PRESENT | I86_PTE_WRITABLE;
    }

    // Register the Page Table in the Page Directory
    // Index 0 in Directory corresponds to Virtual Address 0x00000000 - 0x00400000
    kernel_directory->m_entries[0] = (uint32_t)first_table | I86_PTE_PRESENT | I86_PTE_WRITABLE;

    // --- HEAP MAPPING ---
    // We want the heap to start at 10MB (0xA00000).
    // 10MB falls into the range 8MB-12MB, which is Directory Index 2.
    // Index 0: 0-4MB
    // Index 1: 4-8MB
    // Index 2: 8-12MB
    
    // Allocate a Page Table for the Heap (covers 8MB-12MB)
    page_table* heap_table = (page_table*)pmm_alloc_block();
    if (!heap_table) {
        print_string("VMM Error: Failed to allocate Heap Page Table!\n");
        return;
    }

    // Identity Map 8MB-12MB
    // Physical Addresses: 0x800000 to 0xBFFFFF
    for (int i = 0; i < PAGES_PER_TABLE; i++) {
        // Base address for this table is 8MB (2 * 4MB)
        // Offset i * 4KB
        uint32_t frame = (2 * 1024 * PAGE_SIZE) + (i * PAGE_SIZE);
        
        // Map it!
        heap_table->m_entries[i] = frame | I86_PTE_PRESENT | I86_PTE_WRITABLE;
    }

    // Register this table at Directory Index 2
    kernel_directory->m_entries[2] = (uint32_t)heap_table | I86_PTE_PRESENT | I86_PTE_WRITABLE;

    print_string("VMM: Mapped 8-12MB for Heap.\n");

    print_string("VMM: Identity Mapped 0-4MB.\n");
}

void vmm_enable_paging() {
    // 1. Load CR3 with the address of the page directory
    __asm__ volatile("mov %0, %%cr3" :: "r"(kernel_directory));

    // 2. Read CR0, set the Paging Bit (Bit 31), and write it back
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // Enable Paging
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));

    print_string("VMM: Paging Enabled!\n");
}
