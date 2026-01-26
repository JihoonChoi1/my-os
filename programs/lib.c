
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
