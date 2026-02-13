#ifndef ISR_H
#define ISR_H

#include <stdint.h>

// Registers saved by 'pusha' and the CPU interrupt frame
// Order matches stack layout (Low -> High address)
typedef struct {
    uint32_t gs, fs, es, ds;                         // Data Segment Selectors (Pushed in reverse order)
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    uint32_t eip, cs, eflags, useresp, ss;           // Pushed by the processor automatically
} registers_t;

// Registers saved for Exceptions with Error Code (e.g., Page Fault, GPF)
typedef struct {
    uint32_t gs, fs, es, ds;                         // Data Segment Selectors
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    uint32_t err_code;                               // Pushed by CPU
    uint32_t eip, cs, eflags, useresp, ss;           // Pushed by CPU
} registers_err_t;

// Function prototypes
void isr0_handler();
void page_fault_handler(registers_err_t *regs);

#endif
