// isr.c
#include "idt.h"

// External declaration for the printing function (defined in kernel.c)
extern void print_string(char *str);

// Handler function for Interrupt 0 (Division By Zero)
void isr0_handler()
{
  print_string("\n[!] EXCEPTION: Division By Zero!\n");
  print_string("System Halted.\n");

  // Halt the system with an infinite loop since a fatal error occurred
  while (1)
    ;
}