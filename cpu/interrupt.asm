
; interrupt.asm
[bits 32]

global isr0             ; Make 'isr0' accessible from C code
global isr14            ; Make 'isr14' accessible (Page Fault)
global irq0             ; Make 'irq0' accessible (Timer IRQ)
global irq1             ; Make 'irq1' accessible (Keyboard IRQ)

extern isr0_handler     ; C Handler for Int 0
extern page_fault_handler ; C Handler for Int 14 (Page Fault)
extern timer_handler    ; C Handler for IRQ 0 (Timer)
extern keyboard_handler ; C Handler for IRQ 1 (Keyboard)

; ---------------------------------------------
; Handler for Interrupt 0 (Division By Zero)
; ---------------------------------------------
isr0:
    pusha
    
    ; Save Segments
    push ds
    push es
    push fs
    push gs
    
    ; Load Kernel Data Segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr0_handler
    
    ; Restore Segments
    pop gs
    pop fs
    pop es
    pop ds
    
    popa
    iret

; ---------------------------------------------
; Handler for Interrupt 14 (Page Fault)
; ---------------------------------------------
isr14:
    pusha               ; Save registers
    
    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; Push pointer to regs (registers_err_t*)
    call page_fault_handler
    add esp, 4          ; Pop pointer from stack (CDECL/STDCALL cleanup)
    
    pop gs
    pop fs
    pop es
    pop ds
    
    popa                ; Restore registers
    add esp, 4          ; Pop Error Code (Pushed by CPU for Int 14)
    iret

; ---------------------------------------------
; Handler for IRQ 0 (Timer Interrupt)
; ---------------------------------------------
irq0:
    pusha               ; Save registers

    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call timer_handler
    
    pop gs
    pop fs
    pop es
    pop ds
    
    popa                ; Restore registers
    iret                ; Return from interrupt

; ---------------------------------------------
; Handler for IRQ 1 (Keyboard Interrupt)
; ---------------------------------------------
irq1:
    pusha               ; Save registers

    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call keyboard_handler
    
    pop gs
    pop fs
    pop es
    pop ds
    
    popa                ; Restore registers
    iret                ; Return from interrupt


; ---------------------------------------------
; GDT Flush (Called from gdt.c)
; ---------------------------------------------
global gdt_flush
gdt_flush:
    mov eax, [esp+4]    ; Get GDT pointer (argument)
    lgdt [eax]          ; Load the new GDT pointer

    mov ax, 0x10        ; 0x10 is the offset in GDT to our DATA segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush     ; 0x08 is the offset to our CODE segment (Far Jump)
.flush:
    ret

; ---------------------------------------------
; TSS Flush (Called from tss.c)
; ---------------------------------------------
global tss_flush
tss_flush:
    mov ax, 0x28        ; 0x28 is the offset to our TSS Segment (Index 5 * 8 = 40)
    ltr ax              ; Load Task Register
    ret

; ---------------------------------------------
; Switch to User Mode
; void switch_to_user_mode(void)
; ---------------------------------------------
global switch_to_user_mode
switch_to_user_mode:
    cli
    ; Set Data Segments to User Data Selector (0x20 | 3 = 0x23)
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Setup Stack for IRET to return to Ring 3
    ; Stack Layout for IRET: [SS, ESP, EFLAGS, CS, EIP]
    
    ; Use the Dedicated User Stack at 0xF01000 (Top of the page mapped at 0xF00000)
    ; Stack grows down from 0xF01000 -> 0xF00FFF ...
    mov eax, 0xF01000   
    
    push 0x23           ; SS (User Data Selector with RPL=3)
    push eax            ; ESP (User Stack Top)
    pushf               ; EFLAGS
    
    ; Enable Interrupts in the pushed EFLAGS (Bit 9)
    pop eax
    or eax, 0x200
    push eax
    
    push 0x1B           ; CS (User Code Selector 0x18 | 3 = 0x1B)
    push user_entry    ; EIP (Where to jump)
    
    iret                ; Jump to User Mode!

; -------------------------------------------
; User Data Section
; -------------------------------------------
msg_to_print db "Hello from User Mode! System Call Works!", 0

user_entry:
    ; We are now in User Mode!
    
    ; Call write(stdout, msg)
    mov eax, 1          ; Syscall: WRITE
    mov ebx, 1          ; Arg1: fd (stdout)
    mov ecx, msg_to_print ; Arg2: buffer
    mov edx, 34         ; Arg3: length (ignored for now)
    int 0x80            ; Execute

    ; Call exit(0)
    mov eax, 2          ; Syscall: EXIT
    mov ebx, 0          ; Arg1: exit code
    int 0x80            ; Execute

    ; Infinite loop (Just in case exit fails)
    jmp $

; ---------------------------------------------
; System Call Handler (INT 0x80)
; ---------------------------------------------
extern syscall_handler ; Defined in cpu/syscall.c

global isr128
isr128:
    pusha           ; Save all registers (EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
    
    ; Save Segments
    push ds
    push es
    push fs
    push gs
    
    ; Load Kernel Data Segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Pass register struct (esp now points to gs)
    push esp
    
    call syscall_handler
    
    pop eax         ; Cleanup stack (pop esp arg)
    
; Common Exit Point for System Calls (and Fork)
global isr_exit
isr_exit:
    ; Restore Segments
    pop gs
    pop fs
    pop es
    pop ds
    
    popa            ; Restore registers
    iret            ; Return to User Mode