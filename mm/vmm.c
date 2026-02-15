#include "vmm.h"
#include "pmm.h"

// External helper for printing
extern void print_string(char* str);
extern void print_hex(uint32_t num);

// --- Higher Half Kernel Constants ---
// KERNEL_VIRT_BASE is defined in vmm.h
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

// V2P and P2V are now static inline in vmm.h

// Map a single virtual page to a physical frame
// virt: Virtual Address (Must be Page Aligned)
// phys: Physical Address (Must be Page Aligned)
// flags: Page Flags (Present, RW, User, etc.)
// Map a single virtual page to a physical frame in a specific directory
int vmm_map_page_in_dir(page_directory* dir, uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x03FF;

    // 1. Check if Page Table exists
    if (!(dir->m_entries[pd_index] & I86_PTE_PRESENT)) {
        // Allocate a new Page Table
        uint32_t new_table_phys = pmm_alloc_block();
        if (!new_table_phys) return 0; // OOM

        page_table* new_table_virt = (page_table*)P2V(new_table_phys);
        memset(new_table_virt, 0, sizeof(page_table));

        // Register in Directory
        uint32_t pde_flags = I86_PTE_PRESENT | I86_PTE_WRITABLE;
        if (flags & I86_PTE_USER) {
            pde_flags |= I86_PTE_USER;
        }
        dir->m_entries[pd_index] = new_table_phys | pde_flags;
    }

    // 2. Get the Page Table
    uint32_t table_phys = dir->m_entries[pd_index] & 0xFFFFF000;
    page_table* table = (page_table*)P2V(table_phys);

    // 3. Set the Page Table Entry
    table->m_entries[pt_index] = phys | flags;
    
    // Invalidate TLB if we modified the current address space
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    if (V2P((uint32_t)dir) == cr3) {
        __asm__ volatile("invlpg (%0)" ::"r" (virt) : "memory");
    }

    return 1;
}

// Map a single virtual page to a physical frame (in Kernel Directory)
int vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    return vmm_map_page_in_dir(kernel_directory, virt, phys, flags);
}

// Check if a virtual address is mapped in the directory
int vmm_is_mapped(page_directory* dir, uint32_t virt) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x03FF;

    if (!(dir->m_entries[pd_index] & I86_PTE_PRESENT)) return 0;

    uint32_t table_phys = dir->m_entries[pd_index] & 0xFFFFF000;
    page_table* table = (page_table*)P2V(table_phys);
    
    return (table->m_entries[pt_index] & I86_PTE_PRESENT);
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


// Clone Directory (Updated for copy-on-write fork)
// Returns Physical Address (for CR3)
uint32_t vmm_clone_directory(page_directory* src) {
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

    // 2. Clone User Space (0 ~ 767) - COPY-ON-WRITE
    for (int i = 0; i < 768; i++) {
        if (!(src->m_entries[i] & I86_PTE_PRESENT)) continue;

        // Found a User Page Table. 
        // We allocate a new Page Table for the child, but SHARE the pages.
        
        // A. Allocate New Table
        uint32_t table_phys = pmm_alloc_block();
        if (!table_phys) return 0;
        
        page_table* dst_table = (page_table*)P2V(table_phys);
        memset(dst_table, 0, sizeof(page_table));
        
        // Link Table to Directory
        uint32_t flags = src->m_entries[i] & 0x0FFF;
        dir->m_entries[i] = table_phys | flags;

        // B. Process Pages inside Table
        uint32_t src_table_phys = src->m_entries[i] & 0xFFFFF000;
        page_table* src_table = (page_table*)P2V(src_table_phys);

        for (int j = 0; j < 1024; j++) {
            if (src_table->m_entries[j] & I86_PTE_PRESENT) {
                // Get Frame and Flags
                uint32_t frame_phys = src_table->m_entries[j] & I86_PTE_FRAME;
                uint32_t pte_flags = src_table->m_entries[j] & 0x0FFF;

                // Check if Writable
                if (pte_flags & I86_PTE_WRITABLE) {
                    // Mark Read-Only and set COW bit
                    pte_flags &= ~I86_PTE_WRITABLE;
                    pte_flags |= I86_PTE_COW;
                    
                    // Update SOURCE Table (Parent also loses Write permission!)
                    src_table->m_entries[j] = frame_phys | pte_flags;
                }

                // Increment Reference Count
                pmm_inc_ref(frame_phys);

                // Map in DEST Table (Child)
                dst_table->m_entries[j] = frame_phys | pte_flags;
            }
        }
    }
    
    // FLUSH TLB:
    // We modified the Source Page Table (removed Write permissions).
    // If we are currently running in 'src' directory, we must reload CR3
    // to apply these changes immediately.
    uint32_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    
    // src is Virtual, convert to Physical to compare
    if (V2P((uint32_t)src) == current_cr3) {
        __asm__ volatile("mov %0, %%cr3" :: "r"(current_cr3));
    }

    return dir_phys; // Return Physical Address
}


// Free a directory (for exit/wait)
void vmm_free_directory(page_directory *dir) {
    // Convert Virtual Address to Physical for PMM freeing later
    uint32_t dir_phys = V2P((uint32_t)dir);

    // Iterate User Space (0 ~ 767)
    // Kernel Space (768+) is shared, so we DON'T free it!
    for (int i = 0; i < 768; i++) {
        if (dir->m_entries[i] & I86_PTE_PRESENT) {
            // Found a User Page Table
            uint32_t table_phys = dir->m_entries[i] & I86_PTE_FRAME;
            page_table* table = (page_table*)P2V(table_phys);

            // Iterate Pages inside Table
            for (int j = 0; j < 1024; j++) {
                if (table->m_entries[j] & I86_PTE_PRESENT) {
                    uint32_t frame_phys = table->m_entries[j] & I86_PTE_FRAME;
                    
                    // Free the Frame
                    // pmm_free_block handles refcounting:
                    // If refcount > 1 (COW), it just decrements.
                    // If refcount == 1, it actually frees the bit.
                    pmm_free_block(frame_phys);
                }
            }
            
            // Free the Page Table itself
            // Page Tables are owned by the process, not shared (except for Kernel tables which we skip)
            pmm_free_block(table_phys);
        }
    }

    // Free the Page Directory itself
    pmm_free_block(dir_phys);
}


void vmm_enable_paging() {
    // Paging is already enabled by head.asm.
    // We just update CR3 if needed.
    // For now, do nothing or just reload.
    // To reload kernel_directory:
    // uint32_t phys = V2P((uint32_t)kernel_directory);
    // __asm__ volatile("mov %0, %%cr3" :: "r"(phys));
}
