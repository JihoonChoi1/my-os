#include "syscall.h"
// #include "../kernel/kernel.h" // Removed: Header does not exist yet

// External helper (usually in kernel.c)
extern void print_string(char* str);
extern void print_hex(uint32_t n);
extern void print_dec(uint32_t n);
extern void print_buffer(char* str, unsigned int len);

// Helper for Write
void syscall_write(registers_t *regs) {
    // EAX=1
    // EBX=File Descriptor (1=stdout)
    // ECX=Buffer Pointer
    // EDX=Count

    char* str = (char*)regs->ecx;
    unsigned int len = regs->edx;
    // 1. FD check (only proceed when stdout)
    if (regs->ebx == 1) {
        print_buffer(str, len);
    }
}

extern char keyboard_getchar(); // Blocking read from drivers/keyboard.c
extern uint32_t elf_load(char *filename);
extern void enter_user_mode(uint32_t entry_point);

// Helper for Read (Syscall 0)
void syscall_read(registers_t *regs) {
    // System Call: READ (EAX=0)
    // Arguments follow Linux ABI:
    // EBX = File Descriptor (0 for stdin)
    // ECX = Buffer Pointer (User space address to store data)
    // EDX = Count (Number of bytes to read)

    int fd = regs->ebx;
    char* buf = (char*)regs->ecx;
    
    // Check if the file descriptor is 0 (Standard Input / Keyboard)
    if (fd == 0) {
        *buf = keyboard_getchar();
    }
}

// Helper for Exec (Syscall 3)
void syscall_exec(registers_t *regs) {
    // EAX=3
    // EBX=Filename
    char* filename = (char*)regs->ebx;
    uint32_t entry = elf_load(filename);
    if (entry) {
        enter_user_mode(entry);
    } else {
        // Return -1 on failure
        regs->eax = -1;
    }
}

// Helper for Exit
void syscall_exit(registers_t *regs) {
    // EAX=2
    // EBX=Exit Code
    print_string("\n[Program Exited] Code: ");
    print_dec(regs->ebx);
    
    // In a real OS, we would kill the task.
    // For now, loop forever
    while(1) {
        __asm__ volatile("hlt");
    }
}

void syscall_handler(registers_t *regs) {
    // Dispatch based on EAX
    switch (regs->eax) {
        case 0: // READ
            syscall_read(regs);
            break;
        case 1: // WRITE
            syscall_write(regs);
            break;
        case 2: // EXIT
            syscall_exit(regs);
            break;
        case 3: // EXEC
            syscall_exec(regs);
            break;
        default:
            print_string("Unknown Syscall: ");
            print_dec(regs->eax);
            break;
    }
}
