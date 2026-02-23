// producer_consumer.c
// Demonstrates the Hybrid Semaphore using a classic Producer-Consumer model.
//
// Setup:
//   - 2 Producer threads: each generates 10 items (total: 20 items)
//   - 4 Consumer threads: each consumes 5 items  (total: 20 items)
//   - empty_sem: counts available empty slots (initial = BUFFER_SIZE)
//   - full_sem:  counts items ready to consume (initial = 0)
//   - buffer_lock (mutex): protects concurrent access to head/tail indices
//
// With 2 producers competing for empty slots and 4 consumers competing for
// items, semaphore contention is maximized — showing FIFO fairness and mutex
// necessity clearly.

#include "lib.h"

#define BUFFER_SIZE    5   // Small buffer to maximize blocking/wakeup events
#define PRODUCE_COUNT  10  // Each producer makes 10 items (2 producers = 20 total)
#define CONSUME_COUNT  5   // Each consumer takes  5 items (4 consumers = 20 total)

// --- Shared Circular Buffer ---
volatile int buffer[BUFFER_SIZE];
volatile int buf_head = 0;  // Consumer reads here
volatile int buf_tail = 0;  // Producer writes here

// --- Synchronization Primitives ---
user_sem_t  empty_sem;  // Counts empty slots
user_sem_t  full_sem;   // Counts filled slots
user_mutex_t buf_lock;  // Protects head/tail indices

// --- Thread Stacks (static, 4KB each) ---
char p1_stack[4096], p2_stack[4096];
char c1_stack[4096], c2_stack[4096], c3_stack[4096], c4_stack[4096];

// --- Producer Thread ---
void producer(void *arg)
{
    int id = *(int *)arg;
    for (int i = 0; i < PRODUCE_COUNT; i++) {
        int item = id * 100 + i; // e.g. Producer 1 -> 100~109, Producer 2 -> 200~209

        // 1. Claim an empty slot (blocks if buffer is full)
        sem_wait(&empty_sem);

        // 2. Exclusive access to write into the buffer
        mutex_lock(&buf_lock);
        buffer[buf_tail] = item;
        buf_tail = (buf_tail + 1) % BUFFER_SIZE;
        print("[P");
        print_dec(id);
        print("] Produced: ");
        print_dec(item);
        print("\n");
        mutex_unlock(&buf_lock);

        // 3. Signal that one more item is available
        sem_post(&full_sem);
    }
    print("[P");
    print_dec(id);
    print("] Done.\n");
}

// --- Consumer Thread ---
void consumer(void *arg)
{
    int id = *(int *)arg;
    for (int i = 0; i < CONSUME_COUNT; i++) {
        // 1. Wait until an item is available (blocks if buffer is empty)
        sem_wait(&full_sem);

        // 2. Exclusive access to read from the buffer
        mutex_lock(&buf_lock);
        int item = buffer[buf_head];
        buf_head = (buf_head + 1) % BUFFER_SIZE;
        print("  [C");
        print_dec(id);
        print("] Consumed: ");
        print_dec(item);
        print("\n");
        mutex_unlock(&buf_lock);

        // 3. Signal that one slot is now free
        sem_post(&empty_sem);
    }
    print("  [C");
    print_dec(id);
    print("] Done.\n");
}

int main()
{
    // Initialize synchronization primitives
    sem_init(&empty_sem, BUFFER_SIZE); // All 5 slots are free
    sem_init(&full_sem, 0);            // No items to consume yet
    mutex_init(&buf_lock);

    print("=== Producer-Consumer Demo (2P / 4C) ===\n");
    print("Buffer: 5 | Producers: 2x10 | Consumers: 4x5 | Total: 20 items\n");
    print("-----------------------------------------\n");

    // Thread IDs
    int p1 = 1, p2 = 2;
    int c1 = 1, c2 = 2, c3 = 3, c4 = 4;

    // Spawn 2 producers + 4 consumers (order matters for scheduling)
    thread_create(producer, &p1, p1_stack + 4096);
    thread_create(producer, &p2, p2_stack + 4096);
    thread_create(consumer, &c1, c1_stack + 4096);
    thread_create(consumer, &c2, c2_stack + 4096);
    thread_create(consumer, &c3, c3_stack + 4096);
    thread_create(consumer, &c4, c4_stack + 4096);

    // Wait for all 6 threads to finish
    int status;
    wait(&status); // P1
    wait(&status); // P2
    wait(&status); // C1
    wait(&status); // C2
    wait(&status); // C3
    wait(&status); // C4

    print("-----------------------------------------\n");
    print("=== All threads finished. 20/20 items ===\n");
    exit(0);
}
