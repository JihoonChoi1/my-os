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
#include "../drivers/ata.h"
#include "../fs/simplefs.h"

extern uint32_t _kernel_end;

#define VIDEO_MEMORY 0xC00B8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0f

// VGA Controller Ports: Using Index/Data register structure
// 0x3D4 acts as the "Selector" (Index) and 0x3D5 as the "Value Setter" (Data)
#define REG_SCREEN_CTRL 0x3D4
#define REG_SCREEN_DATA 0x3D5

// COM1 Serial Port (UART) - for mirroring output to Mac terminal via -serial stdio
#define COM1 0x3F8

// Initialize the COM1 serial port (8N1 @ 115200 baud)
void serial_init() {
    port_byte_out(COM1 + 1, 0x00); // Disable interrupts
    port_byte_out(COM1 + 3, 0x80); // Enable DLAB (baud rate divisor mode)
    port_byte_out(COM1 + 0, 0x01); // Baud rate divisor low byte: 1 = 115200 baud
    port_byte_out(COM1 + 1, 0x00); // Baud rate divisor high byte
    port_byte_out(COM1 + 3, 0x03); // 8 data bits, no parity, 1 stop bit (8N1)
    port_byte_out(COM1 + 2, 0xC7); // Enable FIFO, clear, 14-byte threshold
}

// Send one character to COM1 (blocking until transmit buffer is empty)
void serial_putchar(char c) {
    // Wait until the transmit holding register is empty (Line Status bit 5)
    while ((port_byte_in(COM1 + 5) & 0x20) == 0);
    port_byte_out(COM1, c);
    // For newlines, also send \r so terminal cursor returns properly
    if (c == '\n') {
        while ((port_byte_in(COM1 + 5) & 0x20) == 0);
        port_byte_out(COM1, '\r');
    }
}

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

void print_backspace(); // Forward Declaration

void print_buffer(char *string, int len)
{
    // Save current interrupt state and disable interrupts to prevent race condition
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r" (flags));

    for (int i = 0; i < len; i++)
    {
        // Handle newline character (\n)
        if (string[i] == '\n')
        {
            // Calculate the current row index based on the offset
            int current_row = cursor_offset / (2 * MAX_COLS);

            // Move the cursor to the beginning (column 0) of the next row
            cursor_offset = get_screen_offset(0, current_row + 1);
            serial_putchar('\n'); // Mirror to serial
        }
        // Handle Backspace (\b)
        else if (string[i] == '\b')
        {
            print_backspace();
            serial_putchar('\b'); // Mirror to serial
        }
        else
        {
            // Regular character: Print to video memory and advance cursor
            set_char_at_video_memory(string[i], cursor_offset);
            cursor_offset += 2;
            serial_putchar(string[i]); // Mirror to serial
        }

        // Check for scrolling after every character print or newline
        handle_scrolling();
    }

    set_cursor_offset(cursor_offset);

    // Restore interrupt state (if it was enabled before, enable it back)
    if (flags & 0x200) {
        __asm__ volatile("sti");
    }
}

// Print a string and update the hardware cursor position automatically
void print_string(char *string)
{
    int i = 0;

    // Use the global 'cursor_offset' instead of reading from hardware
    while (string[i] != 0) i++;
    print_buffer(string, i);
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
    serial_init(); // Initialize COM1 for mirrored output to host terminal
    clear_screen();
    
    // while(1);
    // 1. Initial messages
    print_string("Phase 1: Bootloader Fixed.\n");
    print_string("Phase 2: Kernel Loaded Successfully.\n");
    print_string("Phase 3: Newline support is now ACTIVE!\n");
    print_string("Phase 4: Scrolling test initiated...\n");
    // while(1);
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
    //while(1);
    // Enable Interrupts
    // Initialize Multitasking
    // Initialize PMM
    //while(1);
    vmm_init();
    //while(1);
    pmm_init((uint32_t)&_kernel_end);
    // Verify PMM
    print_string("--- PMM TEST ---\n");
    uint32_t p1 = pmm_alloc_block();
    print_string("Allocated: 0x"); print_hex(p1); print_string("\n");
    pmm_free_block(p1);
    // Initialize Virtual Memory (Identity Map)

    //while(1);
    // Enable Paging
    //vmm_enable_paging();    

    // Initialize Heap
    kheap_init();

    // TEST: Heap Coalescing (Doubly Linked List Test)
    //print_string("--- HEAP COALESCING TEST ---\n");
    
    // 1. Allocate 3 blocks
    void* ptr_a = kmalloc(256);
    void* ptr_b = kmalloc(256);
    void* ptr_c = kmalloc(256);
    
    // print_string("Allocated A: 0x"); print_hex((uint32_t)ptr_a); print_string("\n");
    // print_string("Allocated B: 0x"); print_hex((uint32_t)ptr_b); print_string("\n");
    // print_string("Allocated C: 0x"); print_hex((uint32_t)ptr_c); print_string("\n");

    // 2. Free Middle Block (B) -> Should just be marked free
    kfree(ptr_b);
    //print_string("Freed B.\n");

    // 3. Free First Block (A) -> Should coalesce with NEXT (B)
    kfree(ptr_a);
    //print_string("Freed A (Should merge with B).\n");

    // 4. Free Last Block (C) -> Should coalesce with PREV (AB)
    kfree(ptr_c);
    //print_string("Freed C (Should merge with AB).\n");

    // 5. Verify: Allocate a big block (size of A+B+C)
    // Size = 256 * 3 + Overhead * 2. 
    // If coalescing worked, it should fit in the starting slot (ptr_a).
    void* ptr_big = kmalloc(256 * 3);
    //print_string("Allocated Big: 0x"); print_hex((uint32_t)ptr_big); print_string("\n");

    // if (ptr_big == ptr_a) {
    //     print_string("SUCCESS! Blocks merged perfectly.\n");
    // } else {
    //     print_string("FAILURE! Fragmentation detected.\n");
    // }
    // print_string("----------------------------\n");
    // --- ATA Driver Test ---
    print_string("Testing ATA Driver...\n");
    uint8_t sect[512];
    ata_read_sector(0, sect); // Read MBR (Sector 0)
    print_string("Read Sector 0. Signature: ");
    print_hex(sect[510]);
    print_string(" ");
    print_hex(sect[511]);
    print_string("\n");

    fs_init();
    
    // Initialize Multitasking (Creates PID 0)
    init_multitasking();
    //while(1);
    // --- Create PID 1: Shell Task ---
    // Instead of transforming the Kernel (PID 0) into Shell via enter_user_mode,
    // we spawn a clean new task (PID 1) to be the Shell.
    // PID 0 will remain as the Idle Process.
    create_task(launch_shell);
    
    // --- Enable Interrupts ---
    // This starts the Timer (IRQ 0), which will trigger the Scheduler.
    // The Scheduler will then pick PID 1 (launch_shell) to run.
    __asm__ volatile("sti");
    
    // --- PID 0: Idle Loop ---
    // The Kernel Main Thread becomes the Idle Process.
    // It runs whenever no other task is ready.
    while(1) {
        // Halt to save power, resume on interrupt
        __asm__ volatile("hlt");
    }
}