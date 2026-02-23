
// lib.c - Minimal C Library for User Programs
#include "lib.h"

// System Call Wrapper
int syscall(int eax, int ebx, int ecx, int edx) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (eax), "b" (ebx), "c" (ecx), "d" (edx)
    );
    return ret;
}

// 1. I/O Functions
char getchar() {
    char c;
    // Syscall 0: READ
    // Arg 1 (EBX): 0 (stdin, keyboard)
    // Arg 2 (ECX): &c (buffer address)
    // Arg 3 (EDX): 1 (num of characters)
    syscall(0, 0, (int)&c, 1);
    return c;
}

void putchar(char c) {
    char str[2] = {c, '\0'};
    // Syscall 1: WRITE
    // Arg 1 (EBX): 1 (stdout, screen)
    // Arg 2 (ECX): &str (buffer address)
    // Arg 3 (EDX): 1 (num of characters)
    syscall(1, 1, (int)str, 1);
}

int strlen(char *s) {
    int i = 0;
    while (s[i] != '\0') i++;
    return i;
}


void print(char *str) {
    int len = strlen(str);
    syscall(1, 1, (int)str, len);
    //while(1);
}

void print_dec(int n) {
    if (n == 0) {
        putchar('0');
        return;
    }

    if (n < 0) {
        putchar('-');
        n = -n;
    }

    char buffer[12];
    int i = 0;
    while (n > 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }

    // Reverse print
    while (i > 0) {
        putchar(buffer[--i]);
    }
}

void print_hex(int n) {
    putchar('0');
    putchar('x');
    if (n == 0) {
        putchar('0');
        return;
    }

    char buffer[10];
    int i = 0;
    while (n > 0) {
        int rem = n % 16;
        if (rem < 10) buffer[i++] = rem + '0';
        else buffer[i++] = (rem - 10) + 'A';
        n /= 16;
    }

    while (i > 0) {
        putchar(buffer[--i]);
    }
}

// 2. String Functions
int strcmp(char *s1, char *s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 0;
        i++;
    }
    return s1[i] - s2[i];
}


// 3. Process Functions
void exit(int code) {
    syscall(2, code, 0, 0);
}

int exec(char *filename) {
    return syscall(3, (int)filename, 0, 0);
}

int fork() {
    return syscall(4, 0, 0, 0);
}

int wait(int *status) {
    return syscall(5, (int)status, 0, 0);
}

// 4. Thread Functions
// thread_create: Create a new thread
// func: Function to run
// arg: Argument to pass to func
// stack: Stack pointer for the new thread
int thread_create(void (*func)(void*), void *arg, void *stack) {
    int *user_stack = (int *)stack;
    
    // Setup initial stack frame for the thread (cdecl calling convention)
    *(--user_stack) = 0;               // dummy argument (for exit)
    *(--user_stack) = (int)arg;        // Argument for func
    *(--user_stack) = (int)exit;       // Return address (thread will call exit() when func returns)
    
    // 1. Call clone system call
    // Syscall 10: CLONE
    // Arg 1 (EBX): Initialized Stack Pointer
    // Arg 2 (ECX): Thread Entry Point
    int ret = syscall(10, (int)user_stack, (int)func, 0);
    
    return ret;
}

// 5. Synchronization Primitives
void spin_lock(volatile int *lock) {
    // Atomic 'xchg' instruction: Writes 1 to lock and returns the previous value.
    // Loops (spins) if the lock was already 1. Exits loop if it was 0 (acquired).
    while (__sync_lock_test_and_set(lock, 1)) {
        // 'pause' instruction: Hardware hint to save power, prevent pipeline 
        // flush penalty, and yield hardware resources to other Hyper-Threads.
        __asm__ volatile("pause");
    }
}

void spin_unlock(volatile int *lock) {
    // Atomic 'mov' instruction: Safely releases the lock by setting it to 0.
    __sync_lock_release(lock);
}

// 6. Hybrid Mutex (Futex-style)
// Uses a 3-state lock value for efficiency:
//   0 = Unlocked
//   1 = Locked (no waiters)
//   2 = Contended (locked + waiters sleeping in kernel)

void mutex_init(user_mutex_t *m) {
    m->lock = 0;
}

void mutex_lock(user_mutex_t *m) {
    // Fast Path: Try to atomically swap 0 -> 1.
    // If successful (old value was 0), we have the lock with zero syscall cost.
    int old = __sync_val_compare_and_swap(&m->lock, 0, 1);
    if (old == 0) return; // Acquired immediately!

    // Slow Path: Lock was already held.
    // Mark as Contended (2) so the unlocker knows to call futex_wake.
    // Then ask the kernel to sleep us until the address changes.
    while (__sync_lock_test_and_set(&m->lock, 2) != 0) {
        // syscall 11 = sys_futex_wait(addr, val)
        // "Sleep me if m->lock is still 2 (contended)"
        syscall(11, (int)&m->lock, 2, 0);
    }
}

void mutex_unlock(user_mutex_t *m) {
    // Atomically fetch the old value and set lock to 0.
    int old = __sync_fetch_and_and(&m->lock, 0);

    // If old was 2 (Contended), there are waiters in the kernel — wake one.
    if (old == 2) {
        // syscall 12 = sys_futex_wake(addr)
        syscall(12, (int)&m->lock, 0, 0);
    }
    // If old was 1 (Locked, no waiters), just returning is enough — no wake needed.
}
