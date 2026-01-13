// idt.h
#ifndef IDT_H
#define IDT_H

// -------------------------------------------------------------------------
// Replacement definitions for standard library (stdint.h)
// -------------------------------------------------------------------------
#include <stdint.h>

// -------------------------------------------------------------------------
// IDT Gate Structure (Represents a single interrupt handler)
// __attribute__((packed)) prevents the compiler from adding padding bytes
// for alignment, ensuring the structure size is exactly 8 bytes.
// -------------------------------------------------------------------------
typedef struct
{
  uint16_t low_offset;  // Lower 16 bits of the handler function address
  uint16_t sel;         // Kernel segment selector (From GDT, usually 0x08)
  uint8_t always0;      // Reserved area, must always be 0
  uint8_t flags;        // Configuration flags (Present, DPL, Type, etc.)
  uint16_t high_offset; // Higher 16 bits of the handler function address
} __attribute__((packed)) idt_gate_t;

// -------------------------------------------------------------------------
// IDT Register Structure
// This struct serves as the pointer passed to the CPU's 'lidt' instruction.
// -------------------------------------------------------------------------
typedef struct
{
  uint16_t limit; // Total size of the IDT table (entries * 8 - 1)
  uint32_t base;  // The starting memory address of the IDT table
} __attribute__((packed)) idt_register_t;

// -------------------------------------------------------------------------
// Variable and Function Declarations
// -------------------------------------------------------------------------
#define IDT_ENTRIES 256

// The actual array where IDT data is stored (used in kernel.c, etc.)
extern idt_gate_t idt[IDT_ENTRIES];
extern idt_register_t idt_reg;

// Function prototypes
void set_idt_gate(int n, uint32_t handler);
void set_idt();

void pic_remap();

#endif