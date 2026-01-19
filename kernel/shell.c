#include "shell.h"
#include "ports.h"
#include "../fs/simplefs.h"
#include "../mm/kheap.h"

// External printing functions from kernel.c
extern void print_string(char *str);
extern void print_char(char c);
extern void print_backspace();
extern void clear_screen(); // From kernel.c

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

// ---------------------------------------------------------
// [Refactor] Command Handler Functions
// ---------------------------------------------------------

void cmd_help() {
    print_string("Available Commands:\n");
    print_string("  help       - Show this message\n");
    print_string("  clear      - Clear the screen\n");
    print_string("  ls         - List files\n");
    print_string("  cat <file> - Print file content\n");
}

void cmd_clear() {
    clear_screen();
}

void cmd_ls() {
    fs_list_files();
}

void cmd_cat(char *filename) {
    // Check if argument is empty
    if (*filename == '\0') {
        print_string("Usage: cat <filename>\n");
        return;
    }

    sfs_inode inode;
    if (fs_find_file(filename, &inode)) {
        // Allocate heap memory for file content
        char *buf = (char*)kmalloc(inode.size + 1);
        if (buf) {
            fs_read_file(&inode, buf);
            buf[inode.size] = '\0'; // Null-terminate
            print_string(buf);
            print_string("\n");
            kfree(buf); // Free memory
        } else {
            print_string("[Error] Out of memory.\n");
        }
    } else {
        print_string("[Error] File not found: ");
        print_string(filename);
        print_string("\n");
    }
}

// ---------------------------------------------------------
// [Refactor] Main Command Executor
// ---------------------------------------------------------
void execute_command(char *input) {
    // 0. Remove trailing spaces
    strip(input);

    // 1. Skip leading spaces
    char *cmd = input;
    while (*cmd == ' ') {
        cmd++;
    }

    // 2. If empty command, return
    if (*cmd == '\0') {
        return;
    }

    // 3. Separate Command and Argument
    // Find where the command ends (space or null)
    char *arg = cmd;
    while (*arg != ' ' && *arg != '\0') {
        arg++;
    }

    // If there is an argument, split the string
    if (*arg != '\0') {
        *arg = '\0'; // Null-terminate the command part
        arg++;       // Move to start of argument part
        while (*arg == ' ') arg++; // Skip spaces before argument
    }
    
    // Now 'cmd' is the command string, 'arg' is the argument string.

    // 4. Dispatch Commands
    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } 
    else if (strcmp(cmd, "clear") == 0) {
        cmd_clear();
    }
    else if (strcmp(cmd, "ls") == 0) {
        cmd_ls();
    }
    else if (strcmp(cmd, "cat") == 0) {
        cmd_cat(arg);
    }
    else {
        print_string("Unknown command: ");
        print_string(cmd);
        print_string("\n");
    }
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
    // 2. Handle Backspace
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
