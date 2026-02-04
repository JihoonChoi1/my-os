; -----------------------------------------------------------------------------
; kernel/head.asm - The Higher Half Kernel Entry Point (Trampoline)
; -----------------------------------------------------------------------------
; This file is the FIRST code that runs when the bootloader jumps to the kernel.
; It is linked at 3GB (Virtual) but loaded at 1MB (Physical).
; Its job is to set up paging so the C kernel can run at 3GB.

[bits 32]

; Define constants
KERNEL_VIRTUAL_BASE equ 0xC0000000                  ; 3GB
KERNEL_PAGE_NUMBER  equ (KERNEL_VIRTUAL_BASE >> 22) ; Page Directory Index for 3GB (768)

section .text
global _start
extern main  ; The C Kernel Main function

_start:
    ; -------------------------------------------------------------------------
    ; 1. Setup Page Directory (The "Manual" Way)
    ; NOTE: We are currently running at PHYSICAL address (approx 1MB), 
    ; but the linker thinks we are at 3GB. We must use physical addresses 
    ; for everything until paging is enabled.
    ; Calculation: Physical = Virtual - 0xC0000000
    ; -------------------------------------------------------------------------

    ; A. Identity Map the First 4MB (0x00000000 -> 0x00000000)
    ; Why? Because when we turn on paging, EIP is still at 1MB. 
    ; We need valid mapping at 1MB to execute the very next instruction.
    ; BootPageDirectory[0] = BootPageTable | Present | RW
    mov eax, (BootPageTable - KERNEL_VIRTUAL_BASE) 
    or eax, 0x3     ; Present | RW
    mov [(BootPageDirectory - KERNEL_VIRTUAL_BASE) + 0], eax

    ; B. Map the Higher Half (0xC0000000 -> 0x00000000)
    ; BootPageDirectory[768] = BootPageTable | Present | RW
    ; This maps Virtual 3GB to Physical 0MB (First 4MB).
    mov [(BootPageDirectory - KERNEL_VIRTUAL_BASE) + (KERNEL_PAGE_NUMBER * 4)], eax

    ; C. Fill the Page Table (Map 0~4MB Physical to the Table)
    ; We fill all 1024 entries of BootPageTable to map 0x0000 ~ 0x3FFFFF (4MB)
    mov ecx, 1024       ; 1024 pages
    mov edi, (BootPageTable - KERNEL_VIRTUAL_BASE) ; Destination
    mov esi, 0          ; Physical Address (Starts at 0)
    mov edx, 3          ; Flags (Present | RW)

.fill_table:
    ; Entry = PhysicalAddr | Flags
    mov eax, esi
    or eax, edx
    mov [edi], eax

    add esi, 4096       ; Next Page (4KB)
    add edi, 4          ; Next Entry (4 bytes)
    loop .fill_table

    ; -------------------------------------------------------------------------
    ; 2. Enable Paging
    ; -------------------------------------------------------------------------
    
    ; Load PDBR (CR3)
    mov eax, (BootPageDirectory - KERNEL_VIRTUAL_BASE)
    mov cr3, eax

    ; Enable Paging (CR0 bit 31)
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; -------------------------------------------------------------------------
    ; 3. Jump to Higher Half
    ; Now paging is ON. Both 1MB (Identity) and 3GB (Higher Half) map to the same code.
    ; We are currently executing at 1MB. We force a jump to 3GB.
    ; -------------------------------------------------------------------------
    lea eax, [_higher_half] ; Load Effective Address of label (Virtual 3GB)
    jmp eax                 ; Absolute Jump

_higher_half:
    ; Unmap the Identity Mapping (0~4MB)
    ; now that we are safely running in the Higher Half.
    mov dword [BootPageDirectory + 0], 0

    ; Flush TLB
    ; The CPU might have cached the "0x0 -> 0x0" mapping in the TLB.
    ; We must reload CR3 to flush the cache and enforce the unmap immediately.
    mov eax, cr3
    mov cr3, eax

    ; Set up Stack (Virtual Address)
    mov esp, stack_top

    ; Call C Kernel
    call main

    ; Hang
    jmp $

; -----------------------------------------------------------------------------
; Data Section (BSS) - Allocated inside the Kernel Binary
; -----------------------------------------------------------------------------
section .bss
align 4096
global BootPageDirectory

; Pre-allocate 4KB for Directory
BootPageDirectory:
    resb 4096

; Pre-allocate 4KB for the First Page Table (0~4MB)
BootPageTable:
    resb 4096

; Kernel Stack (16KB)
stack_bottom:
    resb 16384
stack_top:
