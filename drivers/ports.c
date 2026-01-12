// ports.c
#include "ports.h"

// Read 1 byte from a specific port
unsigned char port_byte_in(unsigned short port)
{
  unsigned char result;
  // "inb %1, %0" : Read from port (dx) into al (result)
  __asm__("inb %1, %0" : "=a"(result) : "d"(port));
  return result;
}

// Write 1 byte to a specific port
void port_byte_out(unsigned short port, unsigned char data)
{
  // "outb %0, %1" : Write value (al/data) to port (dx)
  __asm__("outb %0, %1" : : "a"(data), "d"(port));
}

// Read 2 bytes (Word) from a specific port
unsigned short port_word_in(unsigned short port)
{
  unsigned short result;
  __asm__("inw %1, %0" : "=a"(result) : "d"(port));
  return result;
}

// Write 2 bytes (Word) to a specific port
void port_word_out(unsigned short port, unsigned short data)
{
  __asm__("outw %0, %1" : : "a"(data), "d"(port));
}