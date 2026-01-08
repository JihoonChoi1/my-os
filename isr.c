// isr.c
#include "idt.h"
#include "ports.h" // Added to use port I/O functions

// PIC (Programmable Interrupt Controller) Port Numbers
// Master PIC
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
// Slave PIC
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

// External declaration for the printing functions (defined in kernel.c)
extern void print_string(char *str);
extern void print_hex(int n);

// ----------------------------------------------------------------
// Function to initialize the PIC chips and remap interrupts to 32-47
// ----------------------------------------------------------------
void pic_remap()
{
  // ICW1: Start initialization command (0x11)
  // "Wait for 4 initialization words to be sent in sequence!"
  port_byte_out(PIC1_COMMAND, 0x11);
  port_byte_out(PIC2_COMMAND, 0x11);

  // ICW2: Set the vector offset (Remapping - Critical!)
  // Master PIC starts at interrupt 32 (0x20)
  port_byte_out(PIC1_DATA, 0x20);
  // Slave PIC starts at interrupt 40 (0x28)
  port_byte_out(PIC2_DATA, 0x28);

  // ICW3: Configure Master/Slave cascading
  // Master: "There is a Slave connected to IRQ line 2" (0x04 = 0000 0100)
  port_byte_out(PIC1_DATA, 0x04);
  // Slave: "I am connected to the Master's IRQ line 2" (0x02)
  port_byte_out(PIC2_DATA, 0x02);

  // ICW4: Set environment (8086 mode)
  port_byte_out(PIC1_DATA, 0x01);
  port_byte_out(PIC2_DATA, 0x01);

  // Set Mask: 
  // Master PIC: Enable IRQ 0 (Timer) & IRQ 1 (Keyboard) -> 1111 1100 = 0xFC
  // Slave PIC: Disable all -> 1111 1111 = 0xFF
  port_byte_out(PIC1_DATA, 0xFC);
  port_byte_out(PIC2_DATA, 0xFF);
}

// Handler for Interrupt 0 (Division By Zero)
void isr0_handler()
{
  print_string("\n[!] EXCEPTION: Division By Zero!\n");
  print_string("System Halted.\n");

  // Halt the system with an infinite loop since a fatal error occurred
  while (1)
    ;
}

// Handler for IRQ 1 (Keyboard)
// Moved to keyboard.c
// void keyboard_handler() { ... }