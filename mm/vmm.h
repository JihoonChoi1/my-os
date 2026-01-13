#ifndef VMM_H
#define VMM_H

#include <stdint.h>

/* --- Paging Constants --- */

// Page Table / Directory Entry Flags
#define I86_PTE_PRESENT       0x01  // Page is present in memory
#define I86_PTE_WRITABLE      0x02  // Page is writable
#define I86_PTE_USER          0x04  // User-mode access allowed
#define I86_PTE_WRITETHROUGH  0x08  // Write-through caching
#define I86_PTE_NOT_CACHEABLE 0x10  // Disable caching
#define I86_PTE_ACCESSED      0x20  // Page has been accessed
#define I86_PTE_DIRTY         0x40  // Page has been written to
#define I86_PTE_PAT           0x80  // Page Attribute Table
#define I86_PTE_GLOBAL        0x100 // Global Page (ignored in 4KB)
#define I86_PTE_FRAME         0xFFFFF000 // Frame address mask (Top 20 bits)

// Paging Structure Sizes
#define PAGES_PER_TABLE       1024
#define TABLES_PER_DIRECTORY  1024
#define PAGE_SIZE             4096

/* --- Data Structures --- */

/**
 * Page Table Entry (32-bit)
 * Format:
 * [31...12] Frame Address (Physical Address >> 12)
 * [11...9]  Available for OS
 * [8]       Global
 * [7]       PAT
 * [6]       Dirty
 * [5]       Accessed
 * [4]       Cache Disable
 * [3]       Write Through
 * [2]       User/Supervisor
 * [1]       Read/Write
 * [0]       Present
 */
typedef uint32_t pt_entry;

/**
 * Page Directory Entry (32-bit)
 * Same format as PTE, but points to a Page Table physical address.
 */
typedef uint32_t pd_entry;

/**
 * Page Table
 * Contains 1024 distinct page mappings (covers 4MB).
 */
typedef struct {
    pt_entry m_entries[PAGES_PER_TABLE];
} page_table;

/**
 * Page Directory
 * Contains 1024 pointers to Page Tables (covers 4GB).
 */
typedef struct {
    pd_entry m_entries[TABLES_PER_DIRECTORY];
} page_directory;

/* --- Function Prototypes --- */

// Initialize Virtual Memory Manager
void vmm_init();

// Enable Paging (Load CR3, Set CR0)
void vmm_enable_paging();

// Global Page Directory (Needed for loading CR3)
extern page_directory* kernel_directory;

#endif
