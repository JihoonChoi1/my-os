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
void spin_lock(volatile int *lock);
void spin_unlock(volatile int *lock);

// Hybrid Mutex (Fast Path: user-space atomic, Slow Path: kernel futex)
typedef struct {
    volatile int lock; // 0=Unlocked, 1=Locked, 2=Contended (waiters exist)
} user_mutex_t;

void mutex_init(user_mutex_t *m);
void mutex_lock(user_mutex_t *m);
void mutex_unlock(user_mutex_t *m);

// Hybrid Semaphore
typedef struct {
    volatile int count;
} user_sem_t;

void sem_init(user_sem_t *s, int value);
void sem_wait(user_sem_t *s);
void sem_post(user_sem_t *s);

#endif
