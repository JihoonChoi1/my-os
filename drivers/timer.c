#include "timer.h"
#include "ports.h"

// Reference: https://wiki.osdev.org/Programmable_Interval_Timer
// The PIT's internal frequency is 1.193182 MHz
#define PIT_FREQUENCY 1193182

uint32_t tick = 0;

// Need print functions for debugging
extern void print_string(char *str);
extern void print_dec(int n);

// Defined in process.c
extern void schedule(); 

void timer_handler() {
    tick++;

    // Send EOI to Master PIC (Essential, otherwise system hangs)
    // MUST be sent BEFORE schedule() switches tasks!
    port_byte_out(0x20, 0x20);
    
    // Call Scheduler to switch tasks if needed
    schedule();

    if (tick % 50 == 0) {
        // print_string("Tick() \n");
    }
}

void init_timer(uint32_t freq) {
    // 1. Calculate the divisor
    // The PIT uses a divisor to divide its base frequency (1.19MHz)
    // output_freq = base_freq / divisor
    uint32_t divisor = PIT_FREQUENCY / freq;

    // 2. Send Command Byte to Port 0x43
    // 0x36 = 00 11 011 0
    // Channel 0 (00), Lo/Hi Byte access (11), Mode 3 (Square Wave) (011), Binary (0)
    port_byte_out(0x43, 0x36);

    // 3. Send Divisor (Low Byte then High Byte) to Port 0x40
    uint8_t low = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    port_byte_out(0x40, low);
    port_byte_out(0x40, high);

    print_string("PIT Initialized @ ");
    print_dec(freq);
    print_string("Hz\n");
}
