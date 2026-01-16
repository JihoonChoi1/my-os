// kernel.c - Video driver with Hardware Cursor Control via Port I/O
#include "idt.h"
#include "ports.h"
#include "ports.h"
#include "shell.h"
#include "timer.h"
#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "kheap.h"
#include "gdt.h"
#include "tss.h"

extern uint32_t _kernel_end;

#define VIDEO_MEMORY 0xb8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0f

// VGA Controller Ports: Using Index/Data register structure
// 0x3D4 acts as the "Selector" (Index) and 0x3D5 as the "Value Setter" (Data)
#define REG_SCREEN_CTRL 0x3D4
#define REG_SCREEN_DATA 0x3D5

// Global variable to track the cursor position manually.
// Reading from hardware (port_byte_in) can be unreliable or return garbage data
// on some emulators/boot states, causing the text to print off-screen.
int cursor_offset = 0;

/* --- Helper Function: Memory Copy --- */

/**
 * Copy bytes from one memory location to another.
 * Essential for scrolling (moving lines up).
 * This mimics the standard C library function 'memcpy'.
 */
void memory_copy(char *source, char *dest, int nbytes)
{
    for (int i = 0; i < nbytes; i++)
    {
        *(dest + i) = *(source + i);
    }
}

/* --- String & Conversion Functions --- */

/**
 * Calculate the length of a string.
 */
int strlen(char s[])
{
    int i = 0;
    while (s[i] != '\0')
        ++i;
    return i;
}

/**
 * Reverse a string in-place (e.g., "321" -> "123").
 * Used because the integer conversion algorithm generates digits backwards.
 */
void reverse(char s[])
{
    int c, i, j;
    for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
    {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/**
 * Convert a generic integer (Base 10) to a string.
 */
void int_to_string(int n, char str[])
{
    int i = 0;
    int sign = n;
    if (sign < 0)
        n = -n;

    do
    {
        str[i++] = n % 10 + '0'; // Extract last digit
    } while ((n /= 10) > 0); // Remove last digit

    if (sign < 0)
        str[i++] = '-';

    str[i] = '\0';
    reverse(str);
}

/**
 * Convert an integer to a Hexadecimal string (Base 16).
 * Useful for debugging memory addresses.
 */
void hex_to_string(uint32_t n, char str[])
{
    // Initialize with "0x"
    str[0] = '0';
    str[1] = 'x';

    char *hex_codes = "0123456789ABCDEF";
    int i = 2; // Start writing after "0x"

    if (n == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    // Use a temp buffer because we process digits backwards
    char temp[20];
    int k = 0;

    // Extract hex digits
    do
    {
        temp[k++] = hex_codes[n % 16];
        n /= 16;
    } while (n > 0);

    // Reverse and append to str
    for (int j = k - 1; j >= 0; j--)
    {
        str[i++] = temp[j];
    }
    str[i] = '\0';
}

/* --- Cursor Control Functions --- */

/**
 * Tell the VGA hardware to move the blinking cursor.
 * * Hardware Cursor Index 14: Cursor Location High Byte
 * Hardware Cursor Index 15: Cursor Location Low Byte
 */
void set_cursor_offset(int offset)
{
    // Hardware cursor position is based on character count (0-1999),
    // not byte count. So we divide our memory offset by 2.
    offset /= 2;

    // 1. Tell VGA we want to set the High Byte (Index 14)
    port_byte_out(REG_SCREEN_CTRL, 14);
    // 2. Send the high 8 bits of the offset to the Data port
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));

    // 3. Tell VGA we want to set the Low Byte (Index 15)
    port_byte_out(REG_SCREEN_CTRL, 15);
    // 4. Send the low 8 bits (remaining bits) to the Data port
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}



/* --- High-level Printing Functions --- */

// Write a character and its color attribute to video memory
void set_char_at_video_memory(char character, int offset)
{
    char *vidmem = (char *)VIDEO_MEMORY;
    vidmem[offset] = character;
    vidmem[offset + 1] = WHITE_ON_BLACK;
}

// Convert 2D coordinates (col, row) to a 1D memory offset
int get_screen_offset(int col, int row)
{
    return 2 * (row * MAX_COLS + col);
}

// Wipe the screen by filling it with spaces and reset cursor
void clear_screen()
{
    for (int row = 0; row < MAX_ROWS; row++)
    {
        for (int col = 0; col < MAX_COLS; col++)
        {
            int offset = get_screen_offset(col, row);
            set_char_at_video_memory(' ', offset);
        }
    }
    // Reset our global variable and the hardware cursor
    cursor_offset = 0;
    set_cursor_offset(cursor_offset);
}

/* --- Scrolling Logic --- */

/**
 * Check if the cursor has exceeded the screen size.
 * If so, scroll the text up by one line.
 */
void handle_scrolling()
{
    // If the cursor is beyond the end of the video memory...
    if (cursor_offset >= MAX_ROWS * MAX_COLS * 2)
    {
        // 1. Move all rows up by one.
        // Copy from (Row 1 to End) -> to -> (Row 0)
        int i;
        for (i = 1; i < MAX_ROWS; i++)
        {
            memory_copy(
                (char *)(get_screen_offset(0, i) + VIDEO_MEMORY),     // Source: Row i
                (char *)(get_screen_offset(0, i - 1) + VIDEO_MEMORY), // Dest: Row i-1
                MAX_COLS * 2                                          // Size: One full row
            );
        }

        // 2. Clear the last line (Row 24)
        char *last_line = (char *)(get_screen_offset(0, MAX_ROWS - 1) + VIDEO_MEMORY);
        for (i = 0; i < MAX_COLS * 2; i += 2)
        {
            last_line[i] = ' ';      // Character
            last_line[i + 1] = 0x0f; // Attribute (White on Black)
        }

        // 3. Reset cursor to the start of the last line
        cursor_offset -= 2 * MAX_COLS;
    }
}

// Print a string and update the hardware cursor position automatically
void print_string(char *string)
{
    int i = 0;

    // Use the global 'cursor_offset' instead of reading from hardware
    while (string[i] != 0)
    {
        // Handle newline character (\n)
        if (string[i] == '\n')
        {
            // Calculate the current row index based on the offset
            int current_row = cursor_offset / (2 * MAX_COLS);

            // Move the cursor to the beginning (column 0) of the next row
            cursor_offset = get_screen_offset(0, current_row + 1);
        }
        else
        {
            // Regular character: Print to video memory and advance cursor
            set_char_at_video_memory(string[i], cursor_offset);
            cursor_offset += 2;
        }

        // Check for scrolling after every character print or newline
        handle_scrolling();

        i++;
    }

    // Synchronize the blinking hardware cursor with our text position
    set_cursor_offset(cursor_offset);
}

// Global function to handle backspace visually
void print_backspace() {
    // 1. Check if we are at the beginning of the screen (0)
    // In a real shell, we would also check if we are overwriting the prompt.
    // simpler check: don't delete if offset is 0.
    if (cursor_offset > 0) {
        // 2. Move cursor back by one character (2 bytes)
        cursor_offset -= 2;
        
        // 3. Write a space at the new position to "erase" the character
        set_char_at_video_memory(' ', cursor_offset);
        
        // 4. Update hardware cursor
        set_cursor_offset(cursor_offset);
    }
}

/* --- Printing Wrappers --- */

/**
 * Print a Decimal number (Base 10)
 */
void print_dec(int n)
{
    char str[50];
    int_to_string(n, str);
    print_string(str);
}

/**
 * Print a Hexadecimal number (Base 16)
 */
void print_hex(int n)
{
    char str[50];
    hex_to_string(n, str);
    print_string(str);
}

// Test Tasks defined in tasks.c
extern void task_a();
extern void task_b();

/* --- Main Entry Point --- */

void main()
{
    clear_screen();

    // 1. Initial messages
    print_string("Phase 1: Bootloader Fixed.\n");
    print_string("Phase 2: Kernel Loaded Successfully.\n");
    print_string("Phase 3: Newline support is now ACTIVE!\n");
    print_string("Phase 4: Scrolling test initiated...\n");

    // 2. Fill the screen to trigger scrolling
    // We print many lines to force the text to go beyond row 24.
    int i = 0;
    for (i = 0; i < 21; i++)
    {
        print_string("Filling line for testing...\n");
    }

    print_string("\nCheck\n");

    print_string("Decimal Test (100): ");
    print_dec(100);
    print_string("\n");

    print_string("Hex Test (0x1000): ");
    print_hex(0x1000);
    print_string("\n");

    print_string("Video Memory (0xB8000): ");
    print_hex(VIDEO_MEMORY);
    print_string("\n");

    pic_remap();
    print_string("Phase 4: PIC Remapped (IRQ 0-15 -> INT 32-47).\n");

    set_idt();
    print_string("IDT loaded successfully!\n");

    // Initialize GDT and TSS 
    init_gdt();
    init_tss();
    print_string("GDT & TSS Initialized.\n");
    
    // Initialize Timer (50 Hz)
    init_timer(50);

    // Enable Interrupts
    // Enable Interrupts
    // Initialize Multitasking
    // Initialize PMM
    pmm_init((uint32_t)&_kernel_end);

    // Verify PMM
    print_string("--- PMM TEST ---\n");
    uint32_t p1 = pmm_alloc_block();
    print_string("Allocated: 0x"); print_hex(p1); print_string("\n");
    pmm_free_block(p1);

    // Initialize Virtual Memory (Identity Map)
    vmm_init();
    
    // Enable Paging
    vmm_enable_paging();

    // Initialize Heap
    kheap_init();

    // TEST: Dynamic Allocation
    print_string("TEST: kmalloc(10)\n");
    char* str = (char*)kmalloc(10);
    if (!str) {
        print_string("Malloc failed!\n");
    } else {
        str[0] = 'H'; str[1] = 'e'; str[2] = 'y'; str[3] = '\0';
        print_string("Allocated String: ");
        print_string(str);
        print_string("\n");
        kfree(str);
        print_string("Freed memory.\n");
    }

    // Initialize Multitasking
    init_multitasking();
    create_task(&task_a);
    create_task(&task_b);
    
    // Enable Interrupts
    __asm__ volatile("sti");
    
    // Initialize Shell
    shell_init();

    extern void switch_to_user_mode();
    print_string("\nSwitching to User Mode (Ring 3)...\n");
    switch_to_user_mode();
    print_string("This executes in User Mode (if visible)!\n"); // Should never run if infinite loop in ASM

    while(1) {
        // Wait for interrupt (Saves CPU power)
        __asm__ volatile("hlt");
    }
}