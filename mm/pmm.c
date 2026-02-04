#include "pmm.h"

// External function from kernel.c (or define in a header common to both)
extern void print_string(char* str);
extern void print_hex(uint32_t n);
extern void print_dec(uint32_t n);

// Bitmap to manage 1GB of RAM (1GB / 4KB = 262144 blocks)
// 262144 blocks / 8 bits = 32768 bytes (32KB)
// We statically allocate this in the .bss section.
#define BITMAP_SIZE 32768
static uint8_t memory_bitmap[BITMAP_SIZE];

// Total RAM detected
static uint32_t total_memory_blocks = 0;
static uint32_t used_memory_blocks = 0;

// Helper: Set bit (Mark Used)
void mmap_set(uint32_t bit) {
    memory_bitmap[bit / 8] |= (1 << (bit % 8));
}

// Helper: Unset bit (Mark Free)
void mmap_unset(uint32_t bit) {
    memory_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

// Helper: Test bit (1 = Used, 0 = Free)
int mmap_test(uint32_t bit) {
    return memory_bitmap[bit / 8] & (1 << (bit % 8));
}

// Helper: Find first free block
int mmap_first_free() {
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) { // Iterate through BYTES
        if (memory_bitmap[i] != 0xFF) { // Skip full bytes
             for (int j = 0; j < 8; j++) {
                 int bit = 1 << j;
                 if (!(memory_bitmap[i] & bit)) {
                     return i * 8 + j; // Byte Index * 8 + Bit Offset
                 }
             }
        }
    }
    return -1;
}

void pmm_init(uint32_t kernel_end) {
    // Clear Bitmap (Assume all free initially)
    // safer to mark ALL used, then unmark available regions from E820.
    // mark all as USED first (safety).
    for (int i = 0; i < BITMAP_SIZE; i++) {
        memory_bitmap[i] = 0xFF; // All used
    }

    // Read E820 Map from 0x8000
    // Must access it via the Kernel Base (0xC0000000 + 0x8000).
    // head.asm maps the first 4MB to 0xC0000000, so this works.
    uint16_t entry_count = *(uint16_t*)(0xC0000000 + 0x8000);
    mmap_entry_t* entries = (mmap_entry_t*)(0xC0000000 + 0x8004);

    print_string("PMM: Parsing Memory Map...\n");
    print_string("Entries detected: ");
    print_dec(entry_count);
    print_string("\n");

    uint32_t max_ram = 0;

    for (int i = 0; i < entry_count; i++) {
        mmap_entry_t* entry = &entries[i];
        
        // 32-bit OS safety check: Ignore memory > 4GB to avoid overflow/aliasing
        if (entry->base_high > 0) {
            continue;
        }

        // Type 1 = Usable RAM
        if (entry->type == 1) {
            uint32_t start_addr = entry->base_low;
            uint32_t length = entry->length_low;
            uint32_t end_addr = start_addr + length;
            
            // Checking overflow for 32-bit address space
            if (end_addr < start_addr) {
                end_addr = 0xFFFFFFFF; // Cap at 4GB
            }

            // Update Max RAM
            if (end_addr > max_ram) max_ram = end_addr;

            // Mark these blocks as FREE in bitmap
            // ALIGNMENT FIX:
            // start_block: Round UP (Ceil) to ensure we don't use reserved partial pages at start
            uint32_t start_block = (start_addr + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;
            
            // end_block: Round DOWN (Floor) is default for int division.
            // This ensures we don't use reserved partial pages at end.
            uint32_t end_block = end_addr / PMM_BLOCK_SIZE;

            for (uint32_t j = start_block; j < end_block; j++) {
                if (j < BITMAP_SIZE * 8) { // Safety check
                    mmap_unset(j);
                }
            }
        }
    }
    
    // Calculate total blocks
    total_memory_blocks = max_ram / PMM_BLOCK_SIZE;
    if (total_memory_blocks > BITMAP_SIZE * 8) {
        total_memory_blocks = BITMAP_SIZE * 8; // Cap at bitmap limit
    }

    print_string("Total RAM detected: ");
    print_dec(max_ram / 1024 / 1024);
    print_string(" MB\n");

    // Mark Kernel Region + VMM Static Regions as USED
    // VMM maps:
    // 0-4MB: Kernel Code/Data
    // 4-8MB: User Text
    // 8-12MB: Kernel Heap
    // 12-16MB: User Stack / PMM Frames
    // To prevent PMM from handing out these frames (which would cause aliasing/corruption),
    // we must reserve EVERYTHING up to 16MB.
    
    // Mark Kernel Region as USED
    // Now that we support Higher Half Kernel and dynamic mapping,
    // we only need to reserve the physical memory actually used by the kernel image.
    // The rest (from kernel_end upwards) is free for PMM to allocate.
    
    uint32_t reserved_end = kernel_end; 
    
    // Align up to next block boundary
    uint32_t reserved_limit_block = (reserved_end + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;

    // Protect 0x0 up to Kernel End
    for (uint32_t i = 0; i < reserved_limit_block; i++) {
        mmap_set(i);
    }
    
    print_string("PMM: Reserved Low Memory up to Kernel End.\n");
    
    // [CRITICAL] Reserve the last 16KB for the Kernel Stack (Phase 2 Safety)
    // Detailed Logic: Replicate loader.asm's stack selection to be 100% sure.
    // 1. Loader finds the HIGHEST usable address (which is max_ram).
    // 2. Loader aligns it down to 16 bytes: ESP = max_ram & 0xFFFFFFF0.
    // 3. Stack grows DOWN from there.
    // We must reserve the pages that contain this stack region.
    
    uint32_t stack_top_aligned = max_ram & 0xFFFFFFF0;
    uint32_t stack_bottom = stack_top_aligned - (16 * 1024); // 16KB Stack
    
    // Convert addresses to Block Indices
    // max_ram is the absolute end.
    
    uint32_t start_reserved_block = stack_bottom / PMM_BLOCK_SIZE;
    uint32_t end_reserved_block = total_memory_blocks; // Up to the very end
    
    if (end_reserved_block > start_reserved_block) {
        for (uint32_t i = start_reserved_block; i < end_reserved_block; i++) {
             mmap_set(i);
        }
        print_string("PMM: Reserved Stack from ");
        print_hex(stack_bottom);
        print_string(" to ");
        print_hex(stack_top_aligned);
        print_string("\n");
    }

    // Recalculate used blocks count
    used_memory_blocks = 0;
    for (uint32_t i = 0; i < total_memory_blocks; i++) {
        if (mmap_test(i)) {
            used_memory_blocks++;
        }
    }

    print_string("PMM: Kernel Reserved up to: ");
    print_hex(kernel_end);
    print_string("\n");
}

uint32_t pmm_alloc_block() {
    int frame = mmap_first_free();
    
    if (frame == -1) {
        print_string("Error: Out of Memory!\n");
        return 0; // Return NULL
    }
    
    mmap_set(frame);
    used_memory_blocks++;
    
    uint32_t addr = frame * PMM_BLOCK_SIZE;
    return addr;
}

void pmm_free_block(uint32_t addr) {
    uint32_t frame = addr / PMM_BLOCK_SIZE;
    mmap_unset(frame);
    used_memory_blocks--;
}

void pmm_print_stats() {
    print_string("PMM Stats: Used: ");
    print_dec(used_memory_blocks);
    print_string(" / Total: ");
    print_dec(total_memory_blocks);
    print_string(" blocks\n");
}
