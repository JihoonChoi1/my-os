#include "vmm.h"
#include "pmm.h"

// External helper for printing
extern void print_string(char* str);

// The Kernel's Page Directory
page_directory* kernel_directory = 0;

// Helper: Memory Set
void *memset(void *s, int c, unsigned int n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

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
        
        // Entry = Frame Address | Present | Writable | User (TEMPORARY: Allow Ring 3 to Execute Kernel Code)
        // Since our test code (switch_to_user_mode) is inside the kernel (0-4MB),
        // we must allow User Mode to read/execute pages in this region.
        first_table->m_entries[i] = frame | I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER;
    }

    // Register the Page Table in the Page Directory
    // Index 0 (0-4MB) -> User Accessible
    kernel_directory->m_entries[0] = (uint32_t)first_table | I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER;

    // --- USER PROGRAM TEXT/DATA (4MB - 8MB) ---
    // User programs are linked at 0x400000 (4MB).
    // This corresponds to Directory Index 1.
    
    page_table* user_text_table = (page_table*)pmm_alloc_block();
    if (!user_text_table) {
         print_string("VMM Error: Failed to allocate User Text Table!\n");
         return;
    }
    
    // Map 4MB-8MB. We'll map them as Present | Writable | User.
    // In a real OS, we might want to map these on demand (Page Fault), but for now, 
    // we map the whole 4MB chunk so we can just memcpy the ELF segments there.
    for (int i = 0; i < PAGES_PER_TABLE; i++) {
        uint32_t frame = (1 * 1024 * PAGE_SIZE) + (i * PAGE_SIZE); // Base 4MB
        user_text_table->m_entries[i] = frame | I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER;
    }
    
    // Register in Directory Index 1
    kernel_directory->m_entries[1] = (uint32_t)user_text_table | I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER;
    print_string("VMM: Mapped 4-8MB for User Programs.\n");

    // --- HEAP MAPPING (8MB - 12MB) ---
    // We want the heap to start at 10MB (0xA00000).
    // 10MB falls into the range 8MB-12MB, which is Directory Index 2.
    
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
        uint32_t frame = (2 * 1024 * PAGE_SIZE) + (i * PAGE_SIZE);
        
        // Map it! Kernel Only (Supervisor)
        heap_table->m_entries[i] = frame | I86_PTE_PRESENT | I86_PTE_WRITABLE;
    }

    // Register this table at Directory Index 2
    kernel_directory->m_entries[2] = (uint32_t)heap_table | I86_PTE_PRESENT | I86_PTE_WRITABLE;

    print_string("VMM: Mapped 8-12MB for Heap.\n");

    // --- USER STACK MAPPING (15MB - 16MB) ---
    // We will use Directory Index 3 (12MB - 16MB)
    // 0xF00000 is at 15MB.
    // 15MB is inside the 4MB chunk of Index 3. (Index 3 covers 12MB to 16MB).
    
    page_table* user_stack_table = (page_table*)pmm_alloc_block();
    if (!user_stack_table) {
         print_string("VMM Error: Failed to allocate User Stack Table!\n");
         return;
    }
    
    // 0xF00000 corresponds to the 768th entry in the Page Table of Index 3. 
    // ((15 * 1024 * 1024) % (4 * 1024 * 1024)) / 4096 = (3MB offset) / 4KB = 3072KB / 4KB = 768.
    // Frame: 0xF00000.
    
    // Map 16KB for User Stack (4 Pages)
    // 0xF00000 to 0xF04000
    int user_stack_idx = 768; // 3MB offset into 3rd Page Table (15MB mark)
    
    for (int i = 0; i < 4; i++) {
        uint32_t user_stack_frame = 0xF00000 + (i * PAGE_SIZE);
        user_stack_table->m_entries[user_stack_idx + i] = user_stack_frame | I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER;
    }
    
    // Register in Directory Index 3 (12-16MB)
    kernel_directory->m_entries[3] = (uint32_t)user_stack_table | I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER;
    // Note: PDE must be USER accessible too!

    print_string("VMM: Mapped User Stack at 0xF00000.\n");
}

// Helper to copy content of one physical frame to another
// WARNING: This simply casts physical addresses to pointers.
// It assumes that 'src' and 'dest' are inside the Identity Mapped region (currently 0-12MB is mapped).
// If PMM returns a high address (e.g., > 16MB) that is not mapped, this WILL crash (Page Fault).
// In a full OS, this should use temporary mapping (kmap).
void copy_page_physical(uint32_t src, uint32_t dest) {
    // Using a simple cast since we assume Identity Mapping for implemented ranges
    uint32_t* src_ptr = (uint32_t*)src;
    uint32_t* dest_ptr = (uint32_t*)dest;
    
    for (int i=0; i<1024; i++) {
        dest_ptr[i] = src_ptr[i];
    }
}


// Clone the current page directory (Deep Copy for User, Shared for Kernel)
page_directory* vmm_clone_directory(page_directory* src) {
    // 1. Allocate new Page Directory
    page_directory* dir = (page_directory*)pmm_alloc_block();
    if (!dir) return 0;

    // [Safety] Zero initialization to prevent Triple Faults caused by garbage data
    memset(dir, 0, sizeof(page_directory));

    // 2. Iterate over all 1024 Page Tables
    for (int i = 0; i < TABLES_PER_DIRECTORY; i++) {
        
        // Skip empty entries
        if (!(src->m_entries[i] & I86_PTE_PRESENT)) continue;

        // ---------------------------------------------------------
        // Strategy: Copy vs Share based on Index
        // ---------------------------------------------------------
        
        // [Deep Copy] User Space: Index 1 (Code/Data), Index 3 (Stack)
        // Child process needs its own independent memory for these regions.
        if (i == 1 || i == 3) {
            
            // A. Allocate new Page Table
            page_table* dst_table = (page_table*)pmm_alloc_block();
            if (!dst_table) return 0;
            
            // [Important] Initialize table
            memset(dst_table, 0, sizeof(page_table));

            // Get source physical address (Assumes Identity Mapping)
            page_table* src_table = (page_table*)(src->m_entries[i] & 0xFFFFF000);

            // B. Link in Directory
            // Inherit flags (User/Write, etc.) from parent
            uint32_t flags = src->m_entries[i] & 0x0FFF;
            dir->m_entries[i] = (uint32_t)dst_table | flags;

            // C. Deep Copy Pages inside the table
            for (int j = 0; j < PAGES_PER_TABLE; j++) {
                if (src_table->m_entries[j] & I86_PTE_PRESENT) {
                    
                    // 1. Source Physical Address
                    uint32_t phys_src = src_table->m_entries[j] & 0xFFFFF000;
                    
                    // 2. Allocate new frame for Child
                    uint32_t phys_dst = pmm_alloc_block();
                    if (!phys_dst) return 0; 

                    // 3. Copy Data (Phys -> Phys)
                    // WARNING: This assumes phys_dst is reachable via Identity Mapping (0-4MB or similar).
                    // If PMM returns a high address outside mapped regions, this WILL Page Fault.
                    copy_page_physical(phys_src, phys_dst);

                    // 4. Map into Child Table
                    uint32_t pte_flags = src_table->m_entries[j] & 0x0FFF;
                    dst_table->m_entries[j] = phys_dst | pte_flags;
                }
            }
        }
        
        // [Shared] Kernel Space: Index 0 (Kernel Code), Index 2 (Kernel Heap)
        // Everything else is shared by default to keep kernel state synchronized.
        else {
            dir->m_entries[i] = src->m_entries[i];
        }
    }

    return dir;
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
