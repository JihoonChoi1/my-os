#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>

// Heap Settings
// Start at 10MB to be safe (Kernel is at 1MB, mapped up to 4MB)
#define KHEAP_START         0xA00000
#define KHEAP_INITIAL_SIZE  0x100000  // 1MB Initial Size

// Magic number to check for corruption
#define KHEAP_MAGIC         0x12345678

/**
 * Heap Block Header
 * Every allocated implementation will have this header before the actual data.
 */
typedef struct header {
    struct header *next;   // Pointer to the next block
    struct header *prev;   // Pointer to the previous block (ADDED for Coalescing)
    uint32_t size;         // Size of this block (including header)
    uint32_t magic;        // Magic number for safety check
    uint8_t is_free;       // 1 = Free, 0 = Used
} header_t;

// Function Prototypes
void kheap_init();
void *kmalloc(uint32_t size);
void kfree(void *ptr);

#endif
