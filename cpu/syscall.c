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
// External sys_execve
extern int sys_execve(char *filename, char **argv, char **envp, registers_t *regs);

/* 
// Deprecated: syscall_exec logic moved to helper
void syscall_exec(registers_t *regs) { ... } 
*/

// Helper for Exit
// Removed: syscall_exit implemented in process.c
extern void sys_exit(int code);
extern int sys_wait(int *status);
extern int sys_fork(registers_t *regs);
extern int sys_clone(registers_t *regs);
extern int sys_futex_wait(int *addr, int val);
extern void sys_futex_wake(int *addr);
extern void fs_list_files(); // For SYS_LS (syscall 13)

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
            // EAX = sys_exit(code)
            sys_exit(regs->ebx); 
            break;
        case 3: // EXEC
            // EAX = sys_execve(filename, argv, envp, regs)
            // Currently ignoring argv/envp (NULL)
            regs->eax = sys_execve((char*)regs->ebx, 0, 0, regs); 
            break;
        case 4: // FORK
            // EAX = sys_fork(regs)
            // while(1);
            regs->eax = sys_fork(regs);
            //while(1);
            break;
        case 5: // WAIT
            // EAX = sys_wait(status)
            // status pointer in EBX
            regs->eax = sys_wait((int*)regs->ebx);
            break;
        case 10: // CLONE (Thread Creation)
            // EAX = sys_clone(regs)
            // EBX = Stack Pointer (New Stack)
            regs->eax = sys_clone(regs);
            break;
        case 11: // FUTEX_WAIT
            // EAX = sys_futex_wait(addr, val)
            // EBX = addr (pointer to lock variable in user space)
            // ECX = val (expected value to compare against)
            regs->eax = sys_futex_wait((int*)regs->ebx, (int)regs->ecx);
            break;
        case 12: // FUTEX_WAKE
            // EBX = addr (pointer to lock variable in user space)
            sys_futex_wake((int*)regs->ebx);
            break;
        case 13: // LS — list all files in the filesystem
            fs_list_files();
            break;
        default:
            print_string("Unknown Syscall: ");
            print_dec(regs->eax);
            break;
    }
}
