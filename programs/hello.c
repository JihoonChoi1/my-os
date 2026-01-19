// hello.c - A simple user program
// Since we don't have a C library yet, we define helpers here.

// 1. System Call Wrapper (Inline Assembly)
// Triggers ISR #128 (INT 0x80)
int syscall(int eax, int ebx, int ecx, int edx) {
    int ret;
    __asm__ volatile (
        "int $0x80"             // Trigger interrupt
        : "=a" (ret)            // Output: Return value in EAX
        : "a" (eax),            // Input: EAX (Syscall Number)
          "b" (ebx),            // Input: EBX (Arg 1)
          "c" (ecx),            // Input: ECX (Arg 2)
          "d" (edx)             // Input: EDX (Arg 3)
    );
    return ret;
}

// 2. Helper Functions
void print(char *str) {
    // Syscall #1: WRITE
    // Arguments: 1 (stdout), str, len (ignored by kernel for now)
    syscall(1, 1, (int)str, 0);
}

void exit(int code) {
    // Syscall #2: EXIT
    // Arguments: code
    syscall(2, code, 0, 0);
}

// 3. Entry Point
// This 'main' will be called by our startup code (or directly if simple)
void main() {
    print("Hello from User Space! (Ring 3)\n");
    print("This is a real C program loaded from disk.\n");
    
    // We must call exit(), otherwise execution falls off data (crash)
    exit(0);
}
