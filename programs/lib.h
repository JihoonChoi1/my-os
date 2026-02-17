#ifndef LIB_H
#define LIB_H

int syscall(int eax, int ebx, int ecx, int edx);
char getchar();
void putchar(char c);
void print(char *str);
void print_dec(int n);
void print_hex(int n);
int strcmp(char *s1, char *s2);
int strlen(char *s);
void exit(int code);
int exec(char *filename);
int fork();
int wait(int *status);
int thread_create(void (*func)(void*), void *arg, void *stack);

#endif
