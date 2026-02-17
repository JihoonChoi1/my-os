#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "../mm/vmm.h"

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_TERMINATED,
    PROCESS_BLOCKED // New state for sleeping/waiting
} ProcessState;

typedef struct process {
    uint32_t *esp;       // Stack Pointer (Saved when switching out)
    uint32_t stack[1024]; // 4KB Static Stack for this task
    page_directory *pd;        // Page Directory (Physical Address for CR3)
    uint32_t id;         // Task ID (PID)
    int parent_id;       // Parent Process ID (-1 if none)
    ProcessState state;  // Process State
    int exit_code;       // Exit Status Code
    struct process *next; // Next process in list
    struct process *prev; // Previous process in list
    struct process *wait_next; // Wait Queue (Semaphore/Mutex)
} process_t;

#include "isr.h"

// Globals
extern process_t *current_process;

// Core Process Management
void init_multitasking();
void create_task(void (*function)());
void schedule();
void block_process();
void unblock_process(process_t *p);

// System Calls
int sys_fork(registers_t *regs);
int sys_clone(registers_t *regs); // Kernel Thread
int sys_execve(char *filename, char **argv, char **envp, registers_t *regs);
void sys_exit(int code);
int sys_wait(int *status);

// Other process-related functions
void enter_user_mode(uint32_t entry_point);
void launch_shell();

#endif
