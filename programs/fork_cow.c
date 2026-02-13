#include "lib.h"

int global_var = 100;

void main() {
    print("COW Fork Test Starting...\n");
    print("Parent: global_var = "); print_dec(global_var); print("\n");
    
    int pid = fork();
    
    if (pid == 0) {
        // Child
        print("Child: Created! global_var = "); print_dec(global_var); print("\n");
        print("Child: Writing to global_var (Should trigger COW)...\n");
        global_var = 200;
        print("Child: global_var is now "); print_dec(global_var); print("\n");
        exit(0);
    } else {
        // Parent
        print("Parent: Created Child PID "); print_dec(pid); print("\n");
        print("Parent: Waiting for Child...\n");
        wait(0);
        
        print("Parent: Child Exited.\n");
        print("Parent: global_var = "); print_dec(global_var); print(" (Should be 100)\n");
        
        if (global_var == 100) {
            print("COW TEST PASSED: Parent's memory was isolated.\n");
        } else {
            print("COW TEST FAILED: Parent's memory was corrupted.\n");
        }
        
        exit(0);
    }
}
