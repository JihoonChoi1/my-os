
// lib.c - Minimal C Library for User Programs

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
