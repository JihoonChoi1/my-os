#ifndef TIMER_H
#define TIMER_H

#include "idt.h"

void init_timer(uint32_t freq);
void timer_handler();

#endif
