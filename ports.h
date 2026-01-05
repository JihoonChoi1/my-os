// ports.h
#ifndef PORTS_H
#define PORTS_H

#include "idt.h" // uint8_t 같은 타입 쓰려고 포함

unsigned char port_byte_in(unsigned short port);
void port_byte_out(unsigned short port, unsigned char data);
unsigned short port_word_in(unsigned short port);
void port_word_out(unsigned short port, unsigned short data);

#endif