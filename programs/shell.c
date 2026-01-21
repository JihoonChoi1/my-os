#include "lib.h"

#define MAX_BUFFER 128

void main() {
    char buffer[MAX_BUFFER];
    int index = 0;
    print("Welcome to User Land Shell!\n");
    print("Type 'help' for commands.\n");
    
    while (1) {
        print("> ");
        
        // Input Loop
        index = 0;
        while (1) {
            char c = getchar();
            
            if (c == '\n') {
                print("\n");
                buffer[index] = '\0';
                break;
            } else if (c == '\b') {
                if (index > 0) {
                    index--;
                    // Simple backspace handling: move back, space, move back
                    // Since we don't have full terminal control, we just visually erase
                    // NOTE: This relies on kernel support for backspace visual or implementing it here
                    // For now, let's just print backspace if kernel handles it, or space override
                     print("\b \b"); 
                }
            } else {
                if (index < MAX_BUFFER - 1) {
                    buffer[index++] = c;
                    putchar(c); // Echo
                }
            }
        }

        // Command Processing
        if (index == 0) continue;

        if (strcmp(buffer, "help") == 0) {
            print("Commands: help, exec <file>, exit\n");
        } else if (strcmp(buffer, "exit") == 0) {
            print("Bye!\n");
            exit(0);
        } else {
            // Check for 'exec '
            if (buffer[0] == 'e' && buffer[1] == 'x' && buffer[2] == 'e' && buffer[3] == 'c' && buffer[4] == ' ') {
                 char *program = buffer + 5;
                 print("Executing: ");
                 print(program);
                 print("\n");
                 if (exec(program) == -1) {
                     print("Failed to execute program.\n");
                 }
            } else {
                print("Unknown command: ");
                print(buffer);
                print("\n");
            }
        }
    }
}
