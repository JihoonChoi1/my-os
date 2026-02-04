#include "vmm.h"
#include "pmm.h"

// External helper for printing
extern void print_string(char* str);
extern void print_hex(uint32_t num);

// --- Higher Half Kernel Constants ---
#define KERNEL_VIRT_BASE 0xC0000000
#define KERNEL_PAGE_INDEX (KERNEL_VIRT_BASE >> 22)

// --- Boot Page Directory (Allocated in .bss by head.asm) ---
// Since we are compiled at 0xC0... addresses, &BootPageDirectory gives us a Virtual Address.
extern page_directory BootPageDirectory; 

// The Kernel's Page Directory (Global Pointer)
page_directory* kernel_directory = &BootPageDirectory;

// Bootstrap Page Tables for Direct Mapping (0-128MB)
// We need 32 tables to map 128MB.
// Allocated in .bss (Low Memory Safe Zone), so we can access them during init.
static page_table linear_mapping_tables[32] __attribute__((aligned(4096)));

// Helper: Memory Set
void *memset(void *s, int c, unsigned int n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

// Convert Virtual Address to Physical Address (Simple subtraction)
// Only valid for Kernel Identity Map region (3GB -> 0GB)
uint32_t V2P(uint32_t virt) {
    return virt - KERNEL_VIRT_BASE;
}

// Convert Physical Address to Virtual Address (Simple addition)
uint32_t P2V(uint32_t phys) {
    return phys + KERNEL_VIRT_BASE;
}

// Map a single virtual page to a physical frame
// virt: Virtual Address (Must be Page Aligned)
// phys: Physical Address (Must be Page Aligned)
// flags: Page Flags (Present, RW, User, etc.)
int vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x03FF;

    // 1. Check if Page Table exists
    if (!(kernel_directory->m_entries[pd_index] & I86_PTE_PRESENT)) {
        // Allocate a new Page Table
        // PMM returns a physical address (e.g., 0x200000)
        uint32_t new_table_phys = pmm_alloc_block();
        if (!new_table_phys) return 0; // OOM

        // We need to access this new table to clear it.
        // Since we map all physical RAM to 0xC0xxxxxx (conceptually),
        // we can access it via P2V(new_table_phys).
        page_table* new_table_virt = (page_table*)P2V(new_table_phys);
        memset(new_table_virt, 0, sizeof(page_table));

        // Register in Directory
        // The Entry must store the PHYSICAL address of the table
        uint32_t pde_flags = I86_PTE_PRESENT | I86_PTE_WRITABLE;
        if (flags & I86_PTE_USER) {
            pde_flags |= I86_PTE_USER;
        }
        kernel_directory->m_entries[pd_index] = new_table_phys | pde_flags;
    }

    // 2. Get the Page Table
    // The entry contains the Physical Address of the table.
    uint32_t table_phys = kernel_directory->m_entries[pd_index] & 0xFFFFF000;
    page_table* table = (page_table*)P2V(table_phys);

    // 3. Set the Page Table Entry
    table->m_entries[pt_index] = phys | flags;
    
    // Invalidate TLB for this address
    // The CPU may have cached the previous state (e.g., "Not Present" or an old physical address)
    // in the TLB. We execute 'invlpg' to force the CPU to forget this specific virtual address 
    // and re-read the Page Table from RAM on the next access.
    __asm__ volatile("invlpg (%0)" ::"r" (virt) : "memory");

    return 1;
}


void vmm_init() {
    // Note: Paging is ALREADY Enabled by head.asm!
    // kernel_directory is already pointing to 3GB Virtual Address of BootPageDirectory.

    // 1. Establish Direct Mapping for 0-128MB (Physical) -> 3GB+ (Virtual)
    // This allows the kernel to access ALL physical memory via P2V macro.
    // It also covers the new Kernel Heap location.
    
    // 32 Tables cover 128MB (4MB per table)
    uint32_t start_pde_index = KERNEL_VIRT_BASE >> 22; // Index 768
    
    for (int i = 0; i < 32; i++) {
        // Physical Address of the static table
        uint32_t table_phys = V2P((uint32_t)&linear_mapping_tables[i]);
        //print_hex(table_phys);
        //print_string("\n");
        //print_hex((uint32_t)&linear_mapping_tables[i]);
        //print_string("\n");
        //while(1);
        // Register in Directory
        // linear_mapping_tables[i].m_entries[0] = 1;
        //while(1);
        // Fill the table (Identity Map relative to base)
        for (int j = 0; j < 1024; j++) {
            uint32_t frame_phys = (i * 1024 * 4096) + (j * 4096);
            // while(1);
            linear_mapping_tables[i].m_entries[j] = frame_phys | I86_PTE_PRESENT | I86_PTE_WRITABLE;
        }
        kernel_directory->m_entries[start_pde_index + i] = table_phys | I86_PTE_PRESENT | I86_PTE_WRITABLE;
    }
    uint32_t pd_phys = V2P((uint32_t)kernel_directory);
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys) : "memory");      
    print_string("VMM: Direct Mapping (0-128MB) Established.\n");

    // 2. Map VGA Buffer (Physical 0xB8000) to Virtual 0xC00B8000
    // This allows us to access VGA memory using Higher Half addresses.
    vmm_map_page(0xC00B8000, 0xB8000, I86_PTE_PRESENT | I86_PTE_WRITABLE);
    

    print_string("VMM: Initialized in Higher Half!\n");
    print_string("VMM: Mapped VGA to 0xC00B8000\n");
    print_string("VMM: Mapped Legacy Regions (Heap, User).\n");
}

// Helper: Copy physical page
// We access physical memory by adding KERNEL_VIRT_BASE, because 
// we assume 0~KernelSize is mapped 1:1 at 3GB.
void copy_page_physical(uint32_t src, uint32_t dest) {
    uint32_t* src_ptr = (uint32_t*)P2V(src);
    uint32_t* dest_ptr = (uint32_t*)P2V(dest);
    
    for (int i=0; i<1024; i++) {
        dest_ptr[i] = src_ptr[i];
    }
}


// Clone Directory (Updated for Higher Half)
page_directory* vmm_clone_directory(page_directory* src) {
    // Allocate new directory (Physical)
    uint32_t dir_phys = pmm_alloc_block();
    if (!dir_phys) return 0;

    // Access via Virtual Address
    page_directory* dir = (page_directory*)P2V(dir_phys);
    memset(dir, 0, sizeof(page_directory));

    // 1. Link Kernel Space (768 ~ 1023) - SHARED
    // Everything from 3GB and up is Kernel Space.
    for (int i = 768; i < 1024; i++) {
        dir->m_entries[i] = src->m_entries[i];
    }

    // 2. Clone User Space (0 ~ 767) - DEEP COPY
    for (int i = 0; i < 768; i++) {
        if (!(src->m_entries[i] & I86_PTE_PRESENT)) continue;

        // Found a User Page Table. We need to copy it?
        // Wait, for 'fork', do we copy the TABLE or the PAGES?
        // Classic fork copies the DATA (Pages).
        // Since we are NOT doing COW yet, we must Deep Copy Pages.
        
        // A. Allocate New Table
        uint32_t table_phys = pmm_alloc_block();
        if (!table_phys) return 0;
        
        page_table* dst_table = (page_table*)P2V(table_phys);
        memset(dst_table, 0, sizeof(page_table));
        
        // Link Table to Directory
        uint32_t flags = src->m_entries[i] & 0x0FFF;
        dir->m_entries[i] = table_phys | flags;

        // B. Copy Pages inside Table
        uint32_t src_table_phys = src->m_entries[i] & 0xFFFFF000;
        page_table* src_table = (page_table*)P2V(src_table_phys);

        for (int j = 0; j < 1024; j++) {
            if (src_table->m_entries[j] & I86_PTE_PRESENT) {
                // Allocate New Frame
                uint32_t frame_phys = pmm_alloc_block();
                if (!frame_phys) return 0;

                // Copy Data
                uint32_t src_frame_phys = src_table->m_entries[j] & 0xFFFFF000;
                copy_page_physical(src_frame_phys, frame_phys);

                // Map in New Table
                uint32_t pte_flags = src_table->m_entries[j] & 0x0FFF;
                dst_table->m_entries[j] = frame_phys | pte_flags;
            }
        }
    }

    return dir; // Should return Virtual Address or Physical?
                // The caller expects Virtual address to load into CR3?
                // NO, CR3 needs PHYSICAL. 
                // Wait, this function returns a pointer to struct... normally virtual.
                // But loading CR3 requires V2P translation.
    return dir; 
}


void vmm_enable_paging() {
    // Paging is already enabled by head.asm.
    // We just update CR3 if needed.
    // For now, do nothing or just reload.
    // To reload kernel_directory:
    // uint32_t phys = V2P((uint32_t)kernel_directory);
    // __asm__ volatile("mov %0, %%cr3" :: "r"(phys));
}
