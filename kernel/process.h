#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} ProcessState;

typedef struct process {
    uint32_t *esp;       // Stack Pointer (Saved when switching out)
    uint32_t stack[1024]; // 4KB Static Stack for this task
    uint32_t *pd;        // Page Directory (Physical Address for CR3)
    uint32_t id;         // Task ID (PID)
    int parent_id;       // Parent Process ID (-1 if none)
    ProcessState state;  // Process State
    int exit_code;       // Exit Status Code
    struct process *next; // Linked List Pointer
} process_t;

#include "isr.h"

// Standard process functions
void init_multitasking();
void create_task(void (*function)());
int sys_fork(registers_t *regs);
int sys_execve(char *filename, char **argv, char **envp, registers_t *regs);
void sys_exit(int code);
int sys_wait(int *status);
void schedule();
void enter_user_mode(uint32_t entry_point);

#endif
