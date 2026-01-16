#include "tss.h"
#include "gdt.h" // Need access to gdt array and gdt_set_gate

tss_entry_t tss_entry;

extern void tss_flush(); // Assembly helper to load TR

void init_tss() {
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry) - 1;

    // Add TSS descriptor to GDT (Index 5)
    // Access Byte: 0x89 = 1000 1001b
    //   - P=1 (Present)
    //   - DPL=00 (Ring 0) -> User can't call this directly, but CPU uses it correctly.
    //   - S=0 (System Segment)
    //   - Type=1001 (32-bit Available TSS)
    gdt_set_gate(5, base, limit, 0x89, 0x00);

    // Zero out the TSS
    // (Manual zeroing loop or memset)
    uint8_t *p = (uint8_t *)&tss_entry;
    for (int i=0; i < sizeof(tss_entry); i++) {
        p[i] = 0;
    }

    // Set Kernel Stack Segment (SS0) to Kernel Data (0x10)
    tss_entry.ss0 = 0x10;
    
    // Set Kernel Stack Pointer (ESP0).
    // This value is used when switching from Ring 3 to Ring 0 (e.g., System Call, Interrupt).
    // Currently set to the default boot stack (0x90000).
    // In the future, the Scheduler must update this field for every Task Switch (Context Switch).
    tss_entry.esp0 = 0x90000;

    // Load Task Register
    tss_flush();
}

void tss_set_stack(uint32_t kernel_esp) {
    tss_entry.esp0 = kernel_esp;
}
