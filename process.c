#include "process.h"
#include "ports.h"

// External function (Assembly) to switch context
// void switch_task(uint32_t *next_esp, uint32_t **current_esp_ptr);
extern void switch_task(uint32_t *next_esp, uint32_t **current_esp_ptr);

// Maximum processes
#define MAX_PROCESSES 5

// Process Array (Static TCBs)
static process_t processes[MAX_PROCESSES];
static uint32_t current_pid = 0;
static uint32_t process_count = 0;

extern void print_string(char *str);
extern void print_dec(int n);

// Initialize the process system
void init_multitasking() {
    // 0. Mark all processes as inactive initially
    for (int i=0; i<MAX_PROCESSES; i++) {
        processes[i].active = 0;
    }
    
    // 1. The Kernel itself is Process 0
    processes[0].id = 0;
    processes[0].active = 1;
    current_pid = 0;
    process_count = 1;
    
    print_string("Multitasking Initialized. Kernel is PID 0.\n");
}

// Setup a stack for a new task
// Emulates the stack state as if an interrupt occurred:
// PUSH EFLAGS -> PUSH CS -> PUSH EIP -> PUSHAD (Registers)
void create_task(void (*function)()) {
    if (process_count >= MAX_PROCESSES) {
        print_string("Error: Max Processes Reached.\n");
        return;
    }

    int pid = process_count;
    process_count++;

    processes[pid].id = pid;
    processes[pid].active = 1;

    // 1. Initialize Stack Pointer to the END of the array (Stack grows triggers downwards)
    uint32_t *stack_ptr = &processes[pid].stack[1023];

    // 2. Forge the Stack Content
    // Matching 'switch_task' which does: popf -> popa -> ret
    // So we push Reverse Order: Function(ret) -> Regs(popa) -> Flags(popf)

    // A. Return Address (For 'ret')
    *stack_ptr-- = (uint32_t)function;
    
    // B. General Purpose Registers (For 'popa')
    *stack_ptr-- = 0; // EDI
    *stack_ptr-- = 0; // ESI
    *stack_ptr-- = 0; // EBP
    *stack_ptr-- = 0; // ESP (Ignored)
    *stack_ptr-- = 0; // EBX
    *stack_ptr-- = 0; // EDX
    *stack_ptr-- = 0; // ECX
    *stack_ptr-- = 0; // EAX
    
    // C. EFLAGS (For 'popf')
    // 0x200 = Interrupts Enabled (IF=1)
    *stack_ptr-- = 0x200; 

    // Current ESP points to EFLAGS (the top of our forged stack)
    // Be careful: stack_ptr was decremented AFTER writing 0x200. 
    // So it points to the *next* empty slot (one below 0x200).
    // We want ESP to point TO 0x200.
    processes[pid].esp = stack_ptr + 1;
    
    print_string("Created Task PID ");
    print_dec(pid);
    print_string("\n");
}

// Simple Round-Robin Scheduler
void schedule() {
    // If we only have the kernel (1 process) running, no need to switch
    if (process_count <= 1) return;

    // 1. Get current process info
    int prev_pid = current_pid;
    
    // 2. Select next process (Round-Robin)
    current_pid++;
    if (current_pid >= process_count) {
        current_pid = 0;
    }

    // 3. Perform Context Switch ONLY if the task changed
    if (current_pid != prev_pid) {
        // Switch from Prev -> Next
        // We pass:
        //  1. The new task's ESP (value)
        //  2. A pointer to the old task's ESP storage (so we can save the old ESP)
        switch_task(processes[current_pid].esp, &processes[prev_pid].esp);
    }
}
