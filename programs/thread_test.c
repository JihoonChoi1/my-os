#include "lib.h"

// Shared Global Variable
volatile int counter = 0;

// Synchronization Primitive (Simple Spinlock)
// Since we don't have atomic instructions or sys_mutex yet,
// we'll just demonstrate the RACE CONDITION first.
// If correct, counter should be 30000.
// If race occurs, counter < 30000.

void worker(void *arg)
{
    int id = *(int *)arg;
    print("Thread ");
    print_dec(id);
    print(" starting...\n");

    for (int i = 0; i < 10000; i++)
    {
        // Critical Section (Unprotected!)
        // counter++;
        // To make race more likely, we do: Read -> Delay -> Write
        int temp = counter;

        // Manual Delay loop
        for (int j = 0; j < 100; j++)
        {
            __asm__ volatile("nop");
        }

        counter = temp + 1;
    }

    print("Thread ");
    print_dec(id);
    print(" finished.\n");
    exit(0);
}

// Stacks for threads (static allocation in BSS to avoid Main Stack Overflow)
// 4KB stack per thread
char stack1[4096];
char stack2[4096];
char stack3[4096];

int main()
{
    print("Thread Test: 3 Threads incrementing counter 10000 times.\n");

    int id1 = 1, id2 = 2, id3 = 3;

    // Create Threads
    // Stack grows down, so pass end of array
    int pid1 = thread_create(worker, &id1, stack1 + 4096);
    if (pid1 > 0)
        print("Created Thread 1 (PID ");
    print_dec(pid1);
    print(")\n");

    int pid2 = thread_create(worker, &id2, stack2 + 4096);
    if (pid2 > 0)
        print("Created Thread 2 (PID ");
    print_dec(pid2);
    print(")\n");

    int pid3 = thread_create(worker, &id3, stack3 + 4096);
    if (pid3 > 0)
        print("Created Thread 3 (PID ");
    print_dec(pid3);
    print(")\n");

    // Wait for all threads to finish
    int status;
    wait(&status);
    wait(&status);
    wait(&status);

    print("All threads finished.\n");
    print("Final Counter Value: ");
    print_dec(counter);
    print("\n");
    print("Expected Value: 30000\n");

    if (counter < 30000)
    {
        print("RACE CONDITION DETECTED!\n");
    }
    else
    {
        print("Success? (Or just lucky)\n");
    }

    return 0;
}
