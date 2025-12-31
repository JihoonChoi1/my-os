// kernel.c - Video driver with Hardware Cursor Control via Port I/O

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

/* --- Low-level Port I/O Functions --- */

/**
 * Send 1 byte of data to a specific hardware port.
 * * __asm__ syntax breakdown:
 * - "out %%al, %%dx": Assembly instruction (Output data in AL to port in DX)
 * - "%%al", "%%dx": Double '%' is used to escape and specify literal CPU registers
 * - "a"(data): Binds the C variable 'data' to the 'AL' register
 * - "d"(port): Binds the C variable 'port' to the 'DX' register
 */
void port_byte_out(unsigned short port, unsigned char data)
{
    __asm__("out %%al, %%dx" : : "a"(data), "d"(port));
}

/**
 * Read 1 byte of data from a specific hardware port.
 * * - "=a"(result): After execution, move the value from 'AL' register to 'result'
 */
unsigned char port_byte_in(unsigned short port)
{
    unsigned char result;
    __asm__("in %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
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

        i++;
    }

    // Synchronize the blinking hardware cursor with our text position
    set_cursor_offset(cursor_offset);
}

/* --- Main Entry Point --- */

void main()
{
    clear_screen();

    print_string("Phase 1: Bootloader Fixed.\n");
    print_string("Phase 2: Kernel Loaded Successfully.\n");
    print_string("Phase 3: Newline support is now ACTIVE!\n");
    print_string("\n"); // Empty line for spacing
    print_string("Next Step: Scrolling...");
}