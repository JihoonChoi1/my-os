#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include "process.h"

// 1. Spinlock (Simple Busy Wait / Interrupt Disable)
typedef struct {
    uint32_t locked; // 0=Unlocked, 1=Locked
} spinlock_t;

void spinlock_init(spinlock_t *lock);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);

// 2. Semaphore (Blocking Wait)
typedef struct {
    int value;
    spinlock_t lock;       // Protects the queue
    process_t *wait_head;  // Head of waiting process list (Queue)
    process_t *wait_tail;  // Tail for O(1) append
} semaphore_t;

void sem_init(semaphore_t *sem, int value);
void sem_wait(semaphore_t *sem);   // P() or down()
void sem_signal(semaphore_t *sem); // V() or up()

// 3. Mutex (Binary Semaphore)
typedef struct {
    semaphore_t sem;
    process_t *owner; // Debugging info
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif
