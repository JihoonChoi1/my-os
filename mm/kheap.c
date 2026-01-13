#include "kheap.h"
#include "pmm.h" // For null definition if needed, or just 0
#include "vmm.h" // If we need to map pages dynamically (done statically for now)

extern void print_string(char* str);
extern void print_hex(uint32_t n);

// Start of the heap (Virtual Address)
static uint32_t heap_start = KHEAP_START;
// Current End of the heap
static uint32_t heap_end = KHEAP_START + KHEAP_INITIAL_SIZE;
// Head of the free list
static header_t *free_list = 0;

void kheap_init() {
    // We assume the pages for 10MB-11MB are already mapped by VMM (Identity Mapped).
    // Initialize the first block covering the entire heap
    free_list = (header_t*)heap_start;
    free_list->size = KHEAP_INITIAL_SIZE - sizeof(header_t);
    free_list->magic = KHEAP_MAGIC;
    free_list->is_free = 1;
    free_list->next = 0; // End of list

    print_string("KHEAP: Initialized at 0x");
    print_hex(heap_start);
    print_string(" (Size: 1MB)\n");
}

void *kmalloc(uint32_t size) {
    if (size == 0) return 0;

    // Align size to 4 bytes boundary
    // e.g., size 3 -> 4, size 5 -> 8
    uint32_t aligned_size = (size + 3) & ~3;

    // Find a free block
    header_t *current = free_list;
    header_t *prev = 0;

    while (current) {
        // Check integrity
        if (current->magic != KHEAP_MAGIC) {
            print_string("KHEAP CORRUPTION DETECTED!\n");
            return 0;
        }

        if (current->is_free && current->size >= aligned_size) {
            // Found a fit!
            // Can we split it? We need enough space for a new header + payload
            if (current->size > aligned_size + sizeof(header_t) + 4) {
                // Split logic
                header_t *new_block = (header_t*)((uint32_t)current + sizeof(header_t) + aligned_size);
                
                new_block->magic = KHEAP_MAGIC;
                new_block->is_free = 1;
                new_block->size = current->size - aligned_size - sizeof(header_t);
                new_block->next = current->next;

                // Update current block
                current->size = aligned_size;
                current->next = new_block;
            }
            
            // Mark as used
            current->is_free = 0;
            
            // Return pointer to data (just after header)
            return (void*)((uint32_t)current + sizeof(header_t));
        }

        prev = current;
        current = current->next;
    }

    // No free block found!
    // TODO: Expand heap via VMM (sbrk equivalent)
    print_string("KHEAP: Out of Memory!\n");
    return 0;
}

void kfree(void *ptr) {
    if (!ptr) return;

    // Get header from data pointer
    header_t *block = (header_t*)((uint32_t)ptr - sizeof(header_t));

    // Integrity check
    if (block->magic != KHEAP_MAGIC) {
        print_string("KHEAP: Invalid free pointer!\n");
        return;
    }

    // Mark as free
    block->is_free = 1;

    // Coalesce (Merge) with next block if it's free
    if (block->next && block->next->is_free) {
        block->size += block->next->size + sizeof(header_t);
        block->next = block->next->next;
    }

    // TODO: Coalesce with PREVIOUS block (Requires doubly linked list)
    // For now, this is a simple implementation.
}
