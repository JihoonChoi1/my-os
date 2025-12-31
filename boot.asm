; boot.asm - Step 2: GDT Setup & Preparation for Protected Mode
;
; [System Overview]
; This bootloader performs the essential setup required to switch the CPU
; from 16-bit Real Mode to 32-bit Protected Mode.
;
; [Key Components]
; 1. Stack Initialization (BP/SP -> 0x8000):
;    - Sets the stack pointer safely away from the bootloader code (0x7c00).
;    - Ensures that function calls ('call'/'ret') do not overwrite the executing code.
;
; 2. Output Mechanism (print_string):
;    - A helper function utilizing BIOS interrupt 0x10 (Teletype output).
;    - Verifies that the system is alive before attempting the complex 32-bit switch.
;
; 3. Global Descriptor Table (GDT):
;    - Defines the "Flat Memory Model" allowing access to the full 4GB memory space.
;    - Configures two essential segments:
;      a) Code Segment: Read/Execute, Base=0, Limit=4GB
;      b) Data Segment: Read/Write, Base=0, Limit=4GB
;    - This table is the mandatory "entry ticket" required by the CPU to enable Protected Mode.
[org 0x7c00]
    mov [BOOT_DRIVE], dl

    mov bp, 0x8000  ; Set the base pointer of the stack
    mov sp, bp      ; Set the stack pointer

    mov si, msg_real_mode   ; Load the address of the msg_real_mode
    call print_string        ; Print the string (16-bit BIOS)

    call load_kernel        ; Load kernel(Reading the disk)

    call switch_to_pm       ; Switch to Protected Mode

; ------------------------------------------------------------------
; kernel loading function (BIOS int 0x13 used)
; ------------------------------------------------------------------
load_kernel:
    mov si, msg_load_kernel
    call print_string

    mov bx, 0x1000       ; [destination] address where kernel will get copied
    mov dh, 15           ; [quantity] quantity of sector to be read (1 for now)
    mov dl, [BOOT_DRIVE] ;  restore backuped drive number
    call disk_load       ; -> calls disk load function
    ret

; ------------------------------------------------------------------
; [Function] Disk Load
;
; [BIOS int 0x13 Register Map (Service 0x02 - Read Sectors)]
; AH    = Function Code (0x02 = Read Sectors from Drive)
; AL    = Sector Count (How many 512B sectors to read)
; CH    = Cylinder (0-1023, usually 0 for boot sector)
; CL    = Sector Number (Starts from 1, bits 0-5)
; DH    = Head Number (0-255)
; DL    = Drive Number (Provided by BIOS, e.g., 0x80 for HDD)
; ES:BX = Buffer Address (RAM location where data will be stored)
; ------------------------------------------------------------------
disk_load:
    push dx          ; Store DX on stack so we can recover DH (num sectors) later

    mov ah, 0x02     ; BIOS read sector function
    mov al, dh       ; Read DH sectors
    mov ch, 0x00     ; Select cylinder 0
    mov dh, 0x00     ; Select head 0 (Note: this overwrites the original DH value!)
    mov cl, 0x02     ; Start reading from 2nd sector (i.e. after the boot sector)

    int 0x13         ; BIOS interrupt (Read disk)

    jc disk_error    ; Jump if error (Carry Flag set)

    pop dx           ; Restore DX from the stack
    cmp dh, al       ; Compare sectors requested (DH) with sectors read (AL)
    jne disk_error   ; If different, it's an error
    ret

disk_error:
    mov si, msg_disk_error
    call print_string
    jmp $            ; Jump here forever

; ------------------------------------------------------------------
; string printing constants & variables
; ------------------------------------------------------------------
print_string:
    mov ah, 0x0e      ; BIOS teletype output function
.loop:
    mov al, [si]      ; Load character at [si]
    cmp al, 0         ; Check for null terminator
    je .done

    int 0x10          ; Print character
    add si, 1         ; Move to next character
    jmp .loop
.done:
    ret

msg_real_mode:   db "Started in 16-bit Real Mode...", 0x0a, 0x0d, 0
msg_load_kernel: db "Loading kernel from disk...", 0x0a, 0x0d, 0
msg_disk_error:  db "Disk read error!", 0
BOOT_DRIVE:      db 0

; ------------------------------------------------------------------
; GDT (Global Descriptor Table)
; ------------------------------------------------------------------
gdt_start:
gdt_null:               ; 1. Null descriptor (first 8 bytes must be zero)
    dd 0x0
    dd 0x0
gdt_code:
    ; [Code Segment Descriptor]
    ; Base: 0x0, Limit: 4GB


    dw 0xffff     ; Limit (bits 0-15)
    dw 0x0        ; Base (bits 0-15)
    db 0x0        ; Base (bits 16-23)

    ; Access Byte: 10011010b
    ; -----------------------------------------------------------
    ; 1 (P)   : Present (1 = Valid segment in memory)
    ; 00 (DPL): Descriptor Privilege Level (00 = Ring 0/Kernel)
    ; 1 (S)   : Descriptor Type (1 = Code or Data, 0 = System)
    ; 1 (E)   : Executable (1 = Code segment)
    ; 0 (DC)  : Direction/Conforming (0 = Non-conforming, strictly Ring 0)
    ; 1 (RW)  : Readable (1 = Read access allowed)
    ; 0 (A)   : Accessed (0 = CPU sets this to 1 when accessed)
    db 10011010b

    ; Flags & Limit (16-19): 11001111b
    ; -----------------------------------------------------------
    ; 1 (G)   : Granularity (1 = Limit scaled by 4KB -> 4GB total)
    ; 1 (DB)  : Size Flag (1 = 32-bit Protected Mode)
    ; 0 (L)   : Long Mode (0 = Not 64-bit)
    ; 0 (AVL) : Available for system software (Not used)
    ; 1111    : Limit (bits 16-19)
    db 11001111b

    db 0x0      ; Base (bits 24-31)
gdt_data:
    ; [Data Segment Descriptor]
    ; Same as Code Segment, but Type flags differ

    dw 0xffff
    dw 0x0
    db 0x0

    ; Access Byte: 10010010b (Data, Read/Write)
    ; -----------------------------------------------------------
    ; 1 (P)   : Present
    ; 00 (DPL): Ring 0 (Kernel)
    ; 1 (S)   : Code/Data
    ; 0 (E)   : Executable (0 = Data segment)
    ; 0 (DC)  : Direction (0 = Segment grows up)
    ; 1 (RW)  : Writable (1 = Write access allowed)
    ; 0 (A)   : Accessed
    db 10010010b

    db 11001111b      ; Flags (Same as code: 4KB granularity, 32-bit)
    db 0x0
gdt_end:        ; Label marking the end of GDT

; GDT descriptor record (To tell the CPU where the GDT is located)
gdt_descriptor:
    dw gdt_end - gdt_start - 1    ; Size of GDT (Always size - 1)
    dd gdt_start                  ; Start address of GDT

; Constants for segment offsets
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; ------------------------------------------------------------------
; [Added] Routine to switch to 32-bit Protected Mode
; ------------------------------------------------------------------
[bits 16]
switch_to_pm:
    cli                             ; 1. Disable interrupts (Critical!)
    lgdt [gdt_descriptor]           ; 2. Load the GDT descriptor

    mov eax, cr0                    ; 3. Read CR0 register (Control Register 0)
    or eax, 0x1                     ; 4. Set the PE (Protection Enable) bit to 1
    
    ; The moment we write back to CR0, the CPU switches mode.
    ; Addresses are now interpreted as GDT selectors, not physical segments.
    mov cr0, eax                    ; 5. Write back to CR0 -> **Mode switches to 32-bit here!**


    ; 6. Flush pipeline and switch CS (Far Jump)
    ; We must update CS to our 32-bit Code Segment (CODE_SEG) immediately.
    jmp CODE_SEG:init_pm    


; ------------------------------------------------------------------
; [Added] 32-bit Entry Point (We are now in 32-bit mode!)
; ------------------------------------------------------------------
[bits 32]
init_pm:
    ; 7. Update segment registers to 32-bit data segment (DATA_SEG)
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 8. Move stack to a safe 32-bit free space (0x90000)
    mov ebp, 0x90000
    mov esp, ebp

    ; 9. Verification (Print "PM" to screen using video memory)
    call print_string_pm

    call 0x1000                     ; Jump to Kernel

    jmp $                           ; Infinite loop in 32-bit mode (OS is running)


; [Added] Video memory output function for 32-bit
print_string_pm:
    mov edx, 0xb8000                ; Video memory start address (VGA Text Mode)
    
    mov byte [edx], 'P'             ; First character 'P'
    mov byte [edx+1], 0x0f          ; White text (0x0f) on black background
    
    mov byte [edx+2], 'M'           ; Second character 'M'
    mov byte [edx+3], 0x0f          ; White text (0x0f) on black background
    
    ret

times 510-($-$$) db 0
dw 0xaa55