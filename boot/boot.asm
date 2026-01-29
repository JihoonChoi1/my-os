[org 0x7c00] ; Tell the assembler that the code will be loaded at address 0x7c00
             ; This ensures all labels are calculated with 0x7c00 as the base.

    ; To use function calls in C, or assembly instructions like push, pop, and call, 
    ; a temporary storage space called the Stack is required.
    ; Since 0x7c00 - 0x7e00 is occupied by the bootloader code, 
    ; we use 0x8000 for safety where there is no code.
    mov bp, 0x8000 
    mov sp, bp

    ; BIOS stores the boot drive number in DL on startup.
    ; Persist DL to [BOOT_DRIVE] to avoid data loss during subsequent operations.
    mov [BOOT_DRIVE], dl

    ; Prints message stored in MSG_REAL_MODE.
    ; Why bx? CPU only allows bx, si, di, and bp to be inside square brackets.
    mov bx, MSG_REAL_MODE
    call print_string

    ; Load Stage 2 Loader (loader.bin) from disk
    ; We read 16 sectors starting from Sector 2 (LBA 1)
    ; We load it to address 0x1000.
    mov bx, 0x1000       ; Set the memory offset where data will be stored
    mov dh, 16           ; Specify the number of sectors (512 bytes each) to read
    mov dl, [BOOT_DRIVE] ; Retrieve the stored boot drive ID (saved earlier from BIOS)
    call disk_load       ; Execute the disk read function using the parameters above

    ; Jump to Stage 2
    jmp 0x1000

    jmp $   ; Infinite loop
            ; $ represents the current address, meaning "jump to here" repeatedly.

; --- Data ---
BOOT_DRIVE db 0
MSG_REAL_MODE db "Stage 1 (MBR) Started...", 0x0d, 0x0a, 0
DISK_ERROR_MSG db "Disk read error!", 0

; --- Helper Functions ---
print_string:
    pusha           ; Backup all registers to stack (preserve state)
    mov ah, 0x0e    ; Set BIOS teletype output mode
.loop:
    mov al, [bx]    ; Move the character at address BX into AL
    cmp al, 0       ; Check if the character is 0 (NULL)
    je .done        ; If 0, finish (jump to .done)
    int 0x10        ; Trigger BIOS Interrupt! (Prints character to screen)
    inc bx          ; Move to the next character address
    jmp .loop       ; Repeat
.done:
    popa            ; Restore backed-up registers
    ret             ; Return to caller

disk_load:
    push dx         ; Backup sector count (dh)
    mov ah, 0x02    ; BIOS read mode
    mov al, dh      ; al = number of sectors to read
    mov ch, 0x00    ; Cylinder 0
    mov dh, 0x00    ; Head 0
    mov cl, 0x02    ; Start reading from 2nd sector
    int 0x13        ; Trigger BIOS Disk Interrupt!

    jc disk_error   ; If BIOS fails, it sets the Carry Flag (CF) to 1. 
                    ; We catch errors using jc (Jump if Carry).

    pop dx          ; Restore original target count
    cmp dh, al      ; Check if Actual Read (AL) == Request (DH)
    jne disk_error  ; If different, error (Jump)
    ret             ; Return to caller

disk_error:
    mov bx, DISK_ERROR_MSG
    call print_string
    jmp $

; Padding and Magic Number
times 510-($-$$) db 0   ; times: Repeat the instruction (db 0) this many times.
                        ; 510: The target size. (MBR is 512 bytes, 
                        ; but the last 2 bytes are reserved for the magic number, so we need to fill up to 510).
                        ; $: Current Position. The memory address where the assembler is currently writing.
                        ; $$: Start Position. The memory address where this section (file) began.
dw 0xaa55