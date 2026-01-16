#include "gdt.h"

// Define 6 GDT entries:
// 0: Null
// 1: Kernel Code
// 2: Kernel Data
// 3: User Code
// 4: User Data
// 5: TSS
gdt_entry_t gdt[6];
gdt_ptr_t gp;

extern void gdt_flush(uint32_t); // External assembly function to reload GDT

// Helper to set a GDT gate
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= (granularity & 0xF0);
    gdt[num].access      = access;
}

void init_gdt() {
    gp.limit = (sizeof(gdt_entry_t) * 6) - 1;
    gp.base  = (uint32_t)&gdt;

    // 0: Null Descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Kernel Code (Base=0, Limit=4GB, Access=0x9A, Gran=0xCF)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 2: Kernel Data (Base=0, Limit=4GB, Access=0x92, Gran=0xCF)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 3: User Code (Access=0xFA)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // 4: User Data (Access=0xF2)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // 5: TSS (Initialized later in tss.c, but reserved here)
    gdt_set_gate(5, 0, 0, 0, 0);

    // Reload GDT
    gdt_flush((uint32_t)&gp);
}
