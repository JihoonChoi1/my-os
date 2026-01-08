#include "shell.h"
#include "ports.h"

#define KEYBOARD_DATA_PORT 0x60

// US QWERTY Keyboard Layout (Scancodes 0x00 - 0x39)
char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', // 0x00 - 0x0E
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',   // 0x0F - 0x1B
    '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', // 0x1C - 0x29
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,        // 0x2A - 0x36
    '*', 0, ' '                                                          // 0x37 - 0x39
};

// US QWERTY Keyboard Layout (Shifted)
char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, // 0x00 - 0x0E
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',   // 0x0F - 0x1B
    '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', // 0x1C - 0x29
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,        // 0x2A - 0x36
    '*', 0, ' '                                                          // 0x37 - 0x39
};

extern void print_string(char *str);

// Track Shift Key State (1 = Pressed, 0 = Released)
static int shift_pressed = 0;

void keyboard_handler()
{
    // 1. Read scancode
    unsigned char scancode = port_byte_in(KEYBOARD_DATA_PORT);

    // 2. Handle Shift Key Press (Make Code)
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        goto done;
    }

    // 3. Handle Shift Key Release (Break Code)
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        goto done;
    }

    // 4. Handle Regular Key Press
    if (scancode < 0x80)
    {
        // Ignore undefined keys
        if (scancode > 57) goto done;

        char letter;
        if (shift_pressed) {
            letter = scancode_to_ascii_shift[scancode];
        } else {
            letter = scancode_to_ascii[scancode];
        }

        if (letter != 0) {
            // Send character to Shell Input Buffer
            shell_handle_input(letter);
        }
    }

done:
    // 5. Send EOI to Master PIC
    port_byte_out(0x20, 0x20);
}
