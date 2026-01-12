#include "shell.h"
#include "ports.h"

// External printing functions from kernel.c
extern void print_string(char *str);
extern void print_char(char c);
extern void print_backspace();

#define MAX_BUFFER_SIZE 256
static char input_buffer[MAX_BUFFER_SIZE];
static int buffer_index = 0;

void print_prompt() {
    print_string("> ");
}

void shell_init() {
    print_string("\nWelcome to My Custom Shell!\n");
    print_string("Type something and press Enter.\n");
    buffer_index = 0;
    print_prompt();
}

extern void clear_screen(); // From kernel.c

// Helper: Custom strcmp (0 if equal)
int strcmp(char *s1, char *s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 0;
        i++;
    }
    return s1[i] - s2[i];
}

// Helper: Get string length
int strlen_shell(char *s) {
    int i = 0;
    while (s[i] != '\0') i++;
    return i;
}

// Helper: Remove trailing whitespace
void strip(char *s) {
    int len = strlen_shell(s);
    while (len > 0 && s[len - 1] == ' ') {
        s[len - 1] = '\0';
        len--;
    }
}

// Helper: Custom strncmp (compare up to n chars)
int strncmp(char *s1, char *s2, int n) {
    for (int i=0; i<n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

void execute_command(char *input) {
    // 1. Skip leading spaces
    char *cmd = input;
    while (*cmd == ' ') {
        cmd++;
    }

    // 2. If empty command, return
    if (*cmd == '\0') {
        return;
    }

    // 3. Find end of command (space or null)
    char *end = cmd;
    while (*end != ' ' && *end != '\0') {
        end++;
    }

    // 4. Null-terminate the command string temporarily
    // (This splits "ls -l" into "ls\0-l")
    char saved_char = *end;
    *end = '\0';
    
    // 5. Compare Commands
    if (strcmp(cmd, "help") == 0) {
        print_string("Available Commands:\n");
        print_string("  help   - Show this message\n");
        print_string("  clear  - Clear the screen\n");
        print_string("  ls     - List files (Placeholder)\n");
    } 
    else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    }
    else if (strcmp(cmd, "ls") == 0) {
        print_string("Directory listing:\n");
        print_string("  kernel.bin  (Size: ~4KB)\n");
        print_string("  boot.bin\n");
        print_string("  (SimpleFS Driver not fully linked yet)\n");
    }
    else {
        print_string("Unknown command: ");
        print_string(cmd);
        print_string("\n");
    }

    // Restore the character if needed (for future argument parsing)
    *end = saved_char;
} 


void shell_handle_input(char key) {
    // 1. Handle Newline (Enter Key)
    if (key == '\n') {
        print_string("\n"); 
        
        // Null-terminate the buffer
        input_buffer[buffer_index] = '\0';

        // Execute Command
        execute_command(input_buffer);

        // Reset buffer and show prompt again
        buffer_index = 0;
        print_prompt();
    }
    // 2. Handle Backspace (Enter Key)
    else if (key == '\b') {
        if (buffer_index > 0) {
            // Remove from buffer
            buffer_index--;
            // Remove visually
            print_backspace();
        }
    }
    // 3. Handle Regular Characters
    else {
        if (buffer_index < MAX_BUFFER_SIZE - 1) {
            // Store character
            input_buffer[buffer_index] = key;
            buffer_index++;

            // Echo character to screen so user sees what they type
            char str[2] = {key, '\0'};
            print_string(str);
        }
    }
}
