#ifndef PROCESS_H
#define PROCESS_H

#include "idt.h" // uint32_t, uint8_t definitions

typedef struct {
    uint32_t *esp;       // Stack Pointer (Saved when switching out)
    uint32_t stack[1024]; // 4KB Static Stack for this task
    uint32_t id;         // Task ID (PID)
    uint8_t active;      // 0 = Empty, 1 = Active
} process_t;

// Standard process functions
void init_multitasking();
void create_task(void (*function)());
void schedule();

#endif
