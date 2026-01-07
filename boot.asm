[org 0x7c00]

    ; Set up stack
    mov bp, 0x8000
    mov sp, bp

    ; Determine drive number (DL is set by BIOS)
    mov [BOOT_DRIVE], dl

    ; Print message
    mov bx, MSG_REAL_MODE
    call print_string

    ; Load Stage 2 Loader (loader.bin) from disk
    ; We read 16 sectors starting from Sector 2 (LBA 1)
    ; We load it to address 0x1000.
    mov bx, 0x1000 ; Destination address
    mov dh, 16     ; Number of sectors to read
    mov dl, [BOOT_DRIVE]
    call disk_load

    ; Jump to Stage 2
    jmp 0x1000

    jmp $

; --- Data ---
BOOT_DRIVE db 0
MSG_REAL_MODE db "Stage 1 (MBR) Started...", 0x0d, 0x0a, 0
DISK_ERROR_MSG db "Disk read error!", 0

; --- Helper Functions ---
print_string:
    pusha
    mov ah, 0x0e
.loop:
    mov al, [bx]
    cmp al, 0
    je .done
    int 0x10
    inc bx
    jmp .loop
.done:
    popa
    ret

disk_load:
    push dx
    mov ah, 0x02    ; BIOS read sector function
    mov al, dh      ; Read DH sectors
    mov ch, 0x00    ; Cylinder 0
    mov dh, 0x00    ; Head 0
    mov cl, 0x02    ; Start reading from 2nd sector (Sector 2)
    int 0x13

    jc disk_error

    pop dx
    cmp dh, al      ; if AL (sectors read) != DH (sectors expected)
    jne disk_error
    ret

disk_error:
    mov bx, DISK_ERROR_MSG
    call print_string
    jmp $

; Padding and Magic Number
times 510-($-$$) db 0
dw 0xaa55