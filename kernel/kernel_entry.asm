; kernel_entry.asm - Entry point where the bootloader jumps to the C kernel
[bits 32]           ; We are now in 32-bit Protected Mode
[extern main]       ; Declare that the symbol 'main' is external (defined in the C file)
global _start

_start:
    call main       ; Call the main() function in C kernel
    jmp $           ; Infinite loop (hang) if main returns