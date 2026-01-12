
; interrupt.asm
[bits 32]

global isr0             ; Make 'isr0' accessible from C code
global irq0             ; Make 'irq0' accessible (Timer IRQ)
global irq1             ; Make 'irq1' accessible (Keyboard IRQ)

extern isr0_handler     ; C Handler for Int 0
extern timer_handler    ; C Handler for IRQ 0 (Timer)
extern keyboard_handler ; C Handler for IRQ 1 (Keyboard)

; ---------------------------------------------
; Handler for Interrupt 0 (Division By Zero)
; ---------------------------------------------
isr0:
    pusha
    call isr0_handler
    popa
    iret

; ---------------------------------------------
; Handler for IRQ 1 (Keyboard Interrupt)
; ---------------------------------------------
irq0:
    pusha               ; Save registers
    call timer_handler
    popa                ; Restore registers
    iret                ; Return from interrupt

irq1:
    pusha               ; Save registers
    call keyboard_handler
    popa                ; Restore registers
    iret                ; Return from interrupt
; ---------------------------------------------
; Context Switching Function (Called from C)
; void switch_task(uint32_t *next_esp, uint32_t **current_esp_ptr);
; ---------------------------------------------
global switch_task
switch_task:
    ; 1. Save Registers of the Current Task
    pusha               ; Push EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    pushf               ; Push EFLAGS

    ; 2. Save Current Task's ESP
    ; Arguments: [ESP+40] = next_esp, [ESP+44] = current_esp_ptr
    ; Note: pusha (32) + pushf (4) + return addr (4) = 40 bytes offset to first arg
    mov eax, [esp + 44] ; Get address of current_esp_ptr (pointer to a pointer)
    mov [eax], esp      ; Store the current ESP value into that pointer

    ; 3. Load Next Task's ESP
    mov eax, [esp + 40] ; Get next_esp (value)
    mov esp, eax        ; Overwrite ESP with new stack!

    ; 4. Restore Registers of the Next Task
    popf                ; Restore EFLAGS
    popa                ; Restore General Purpose Registers

    ret                 ; Jump to the next task's code!