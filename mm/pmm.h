#ifndef PMM_H
#define PMM_H

#include <stdint.h>

// 4KB Block Size
#define PMM_BLOCK_SIZE 4096

// E820 Memory Map Entry Structure
typedef struct {
    uint32_t base_low;
    uint32_t base_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
    uint32_t acpi_attrs; // ACPI 3.0
} __attribute__((packed)) mmap_entry_t;

void pmm_init(uint32_t kernel_end);
uint32_t pmm_alloc_block();
void pmm_free_block(uint32_t addr);

// Debug function to print memory stats
void pmm_print_stats();

#endif
