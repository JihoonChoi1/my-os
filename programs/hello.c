#include "lib.h"

// hello.c - A simple user program
// Now uses the shared library (lib.c)

// 3. Entry Point
// This 'main' will be called by our startup code (or directly if simple)
void main() {
    print("Hello from User Space! (Ring 3)\n");
    print("This is a real C program loaded from disk.\n");
    
    // We must call exit(), otherwise execution falls off data (crash)
    exit(0);
}
