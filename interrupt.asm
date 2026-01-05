; interrupt.asm
[bits 32]

global isr0             ; Make 'isr0' accessible from C code (e.g., idt.c)
extern isr0_handler     ; External symbol for the C handler defined in isr.c

; ---------------------------------------------
; Handler for Interrupt 0 (Division By Zero)
; ---------------------------------------------
isr0:
    pusha               ; 1. Save all general-purpose registers (EAX, EBX, etc.)
                        ;    (Preserves CPU state in case C code modifies registers)

    call isr0_handler   ; 2. Call the C kernel handler function!

    popa                ; 3. Restore the saved registers
    iret                ; 4. Return from interrupt and resume original execution (Essential!)