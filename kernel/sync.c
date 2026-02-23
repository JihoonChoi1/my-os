#include "sync.h"

// --- IRQ Lock Implementation ---
// On a single-core system (UP), locking = disabling interrupts (cli/sti).
// This is NOT a true spinlock — it does not busy-wait.

void irq_lock_init(irq_lock_t *lock) {
    lock->locked = 0;
}

void irq_lock(irq_lock_t *lock) {
    __asm__ volatile("cli");
    // In SMP, we would spin here: while (__sync_lock_test_and_set(&lock->locked, 1));
    lock->locked = 1; 
}

void irq_unlock(irq_lock_t *lock) {
    lock->locked = 0;
    __asm__ volatile("sti");
}

// --- Semaphore Implementation ---

extern void block_process();
extern void schedule();
extern void unblock_process(process_t *p);
extern process_t *current_process; // Need access to current process

void sem_init(semaphore_t *sem, int value) {
    sem->value = value;
    irq_lock_init(&sem->lock);
    sem->wait_head = 0;
    sem->wait_tail = 0;
}

void sem_wait(semaphore_t *sem) {
    while (1) {
        irq_lock(&sem->lock); // Disable Interrupts

        if (sem->value > 0) {
            sem->value--;
            irq_unlock(&sem->lock); // Enable Interrupts
            return;
        }

        // Value is 0. We must wait.
        // 1. Add current process to wait queue
        current_process->wait_next = 0;
        if (sem->wait_head == 0) {
            sem->wait_head = current_process;
            sem->wait_tail = current_process;
        } else {
            sem->wait_tail->wait_next = current_process;
            sem->wait_tail = current_process;
        }

        // Critical Section: Add to queue -> Unlock -> Sleep
        // We must release the logical lock so signal() can work,
        // but keep interrupts disabled (CLI) to prevent Lost Wakeup.
        sem->lock.locked = 0; 
        
        current_process->state = PROCESS_BLOCKED;
        
        // Context Switch (Re-enables interrupts in next task)
        schedule(); 

        // Woken up by signal(). 
        // Loop back to re-check value because another process might have stolen it (Mesa Semantics).
    }
}

void sem_signal(semaphore_t *sem) {
    irq_lock(&sem->lock);

    sem->value++;

    if (sem->wait_head != 0) {
        // Wake up the first process in the queue
        process_t *waking_process = sem->wait_head;
        sem->wait_head = sem->wait_head->wait_next;
        
        if (sem->wait_head == 0) {
            sem->wait_tail = 0;
        }
        
        unblock_process(waking_process);
    }

    irq_unlock(&sem->lock);
}

// --- Mutex Implementation ---

void mutex_init(mutex_t *mutex) {
    sem_init(&mutex->sem, 1); // Binary Semaphore (starts at 1)
    mutex->owner = 0;
}

void mutex_lock(mutex_t *mutex) {
    sem_wait(&mutex->sem);
    mutex->owner = current_process;
}

void mutex_unlock(mutex_t *mutex) {
    // Safety Check: Only the owner can unlock
    if (mutex->owner != current_process) {
        return; 
    }
    
    mutex->owner = 0;
    sem_signal(&mutex->sem);
}
