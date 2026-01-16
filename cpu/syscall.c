#include "syscall.h"
// #include "../kernel/kernel.h" // Removed: Header does not exist yet

// External helper (usually in kernel.c)
extern void print_string(char* str);
extern void print_hex(uint32_t n);
extern void print_dec(uint32_t n);

// Helper for Write
void syscall_write(registers_t *regs) {
    // EAX=1
    // EBX=File Descriptor (1=stdout)
    // ECX=Buffer Pointer
    // EDX=Count

    char* str = (char*)regs->ecx;
    // For now, ignore FD and Count, just print string
    // Security TODO: Check if pointer is in User Space
    print_string(str);
}

// Helper for Exit
void syscall_exit(registers_t *regs) {
    // EAX=2
    // EBX=Exit Code
    print_string("\n[Program Exited] Code: ");
    print_dec(regs->ebx);
    
    // In a real OS, we would kill the task.
    // For now, loop forever
    while(1);
}

void syscall_handler(registers_t *regs) {
    // Dispatch based on EAX
    switch (regs->eax) {
        case 1: // WRITE
            syscall_write(regs);
            break;
        case 2: // EXIT
            syscall_exit(regs);
            break;
        default:
            print_string("Unknown Syscall: ");
            print_dec(regs->eax);
            break;
    }
}
