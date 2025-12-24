; boot.asm - My First Bootloader
; 
; Logic Explanation:
; 1. The BIOS loads this code into memory at 0x7c00.
; 2. We use BIOS interrupt 0x10 with AH=0x0e to print characters.
; 3. We pad the file to 512 bytes and add the boot signature.

[org 0x7c00]    ; All commands act as if they are at memory 0x7c00

mov ah, 0x0e    ; Set AH to 0x0e (Teletype mode: print char to screen)

mov al, 'H'     ; Put 'H' into AL
int 0x10        ; Call BIOS interrupt (BIOS checks AH and acts)

mov al, 'e'
int 0x10

mov al, 'l'
int 0x10

mov al, 'l'
int 0x10

mov al, 'o'
int 0x10

jmp $           ; Jump to current position (Infinite Loop)

; Padding and Signature
times 510-($-$$) db 0  ; Fill the rest with zeros until 510th byte
dw 0xaa55              ; The Magic Number (Boot Signature)