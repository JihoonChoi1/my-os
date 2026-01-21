#ifndef LIB_H
#define LIB_H

int syscall(int eax, int ebx, int ecx, int edx);
char getchar();
void putchar(char c);
void print(char *str);
int strcmp(char *s1, char *s2);
int strlen(char *s);
void exit(int code);
int exec(char *filename);

#endif
