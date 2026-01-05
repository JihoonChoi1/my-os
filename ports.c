// ports.c
#include "ports.h"

// 포트에서 1바이트 읽기 (inb)
unsigned char port_byte_in(unsigned short port)
{
  unsigned char result;
  // "inb %1, %0" : 포트(dx)에서 값을 읽어 al(result)에 넣어라
  __asm__("inb %1, %0" : "=a"(result) : "d"(port));
  return result;
}

// 포트에 1바이트 쓰기 (outb)
void port_byte_out(unsigned short port, unsigned char data)
{
  // "outb %0, %1" : 값(al/data)을 포트(dx)에 써라
  __asm__("outb %0, %1" : : "a"(data), "d"(port));
}

// 포트에서 2바이트(Word) 읽기
unsigned short port_word_in(unsigned short port)
{
  unsigned short result;
  __asm__("inw %1, %0" : "=a"(result) : "d"(port));
  return result;
}

// 포트에 2바이트(Word) 쓰기
void port_word_out(unsigned short port, unsigned short data)
{
  __asm__("outw %0, %1" : : "a"(data), "d"(port));
}