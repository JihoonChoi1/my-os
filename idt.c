// idt.c
#include "idt.h"

extern void isr0();

// 1. Define the actual variables here (Allocates memory)
idt_gate_t idt[IDT_ENTRIES];
idt_register_t idt_reg;

// 2. Function to configure a single IDT gate (Interrupt Handler Entry)
void set_idt_gate(int n, uint32_t handler)
{
  // Lower 16 bits of the handler function address
  idt[n].low_offset = handler & 0xFFFF;

  // Kernel code segment selector (0x08, as defined in GDT)
  idt[n].sel = 0x08;

  // Reserved area, must always be 0
  idt[n].always0 = 0;

  // Set flags (0x8E = 10001110 binary)
  // P=1 (Present), DPL=00 (Kernel Privilege), Type=1110 (32-bit Interrupt Gate)
  idt[n].flags = 0x8E;

  // Upper 16 bits of the handler function address
  idt[n].high_offset = (handler >> 16) & 0xFFFF;
}

// 3. Function to load the IDT into the CPU
void set_idt()
{
  // Configure the IDT Pointer (Register)
  idt_reg.base = (uint32_t)&idt;
  idt_reg.limit = IDT_ENTRIES * sizeof(idt_gate_t) - 1;

  // Register the handler for Interrupt 0 (Division By Zero)
  set_idt_gate(0, (uint32_t)isr0);

  // Execute "lidt" instruction (Load IDT)
  // Using inline assembly to execute assembly instructions within C code.
  // We pass the address of 'idt_reg' to the CPU to tell it where the IDT is located.
  __asm__ volatile("lidt (%0)" : : "r"(&idt_reg));
}