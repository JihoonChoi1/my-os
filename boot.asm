; boot.asm - Step 2: Stack Setup & String Printing Function
;
; [Change Log]
; 1. Stack Initialization (BP/SP -> 0x8000):
;    - Created a safe stack space away from the bootloader code (0x7c00).
;    - This is essential for using 'call'/'ret' instructions and preparing for C code.
;
; 2. Function Implementation (print_string):
;    - Replaced repetitive manual 'int 0x10' calls with a reusable loop.
;    - Uses SI register as a pointer to iterate through memory.
;    - Implements C-style null-terminated string logic (stops when it hits 0).

[org 0x7c00]  


mov bp, 0x8000 
mov sp, bp     

; [2] 문자열 출력 준비
mov si, msg_hello  
call print_string  

jmp $           


print_string:
    mov ah, 0x0e      
.loop:
    mov al, [si]    
    cmp al, 0       
    je .done         
    
    int 0x10          
    add si, 1         
    jmp .loop         
.done:
    ret             

msg_hello: db 'Hello, Operating System World!', 0 

times 510-($-$$) db 0
dw 0xaa55