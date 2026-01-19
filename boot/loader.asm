[org 0x1000]

start:
    ; Save Boot Drive ID
    mov [BOOT_DRIVE], dl

    ; Print start message
    mov si, msg_stage2
    call print_string

    ; Enable A20 Line
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Enter Unreal Mode
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp $+2
    mov bx, 0x08
    mov ds, bx
    mov es, bx
    mov eax, cr0
    and al, 0xFE
    mov cr0, eax
    jmp $+2
    xor ax, ax
    mov ds, ax
    mov es, ax
    sti

    mov si, msg_unreal
    call print_string

    ; Read Superblock (LBA 17) -> 0x2000
    mov si, dap
    mov word [dap.lba_low], 17
    mov word [dap.offset], 0x2000
    mov dl, [BOOT_DRIVE]
    mov ah, 0x42
    int 0x13
    jc disk_error

    mov si, msg_read_sb
    call print_string

    ; Verify Magic
    mov eax, [0x2000]
    cmp eax, 0x12345678
    jne magic_error
    mov si, msg_magic_ok
    call print_string

    ; Find "kernel.bin"
    mov eax, [0x200C]
    mov [inode_table_lba], eax

    mov si, dap
    mov eax, [inode_table_lba]
    mov [dap.lba_low], eax
    mov word [dap.offset], 0x3000
    mov dl, [BOOT_DRIVE]
    mov ah, 0x42
    int 0x13
    jc disk_error

    mov di, 0x3000
    mov cx, 5
.find_loop:
    mov al, [di]
    cmp al, 1
    jne .next_inode

    push di
    inc di
    mov si, filename_target
    call strcmp
    pop di
    
    je .found_kernel

.next_inode:
    add di, 256    ; sizeof(sfs_inode) is now 256 bytes
    loop .find_loop
    jmp .kernel_not_found

.kernel_not_found:
    mov si, msg_kernel_not_found
    call print_string
    jmp $

.found_kernel:
    mov si, msg_kernel_found
    call print_string

    ; Save Inode Pointer because we need EDI for destination (and DI is lower 16-bit of EDI)
    mov [inode_ptr], di

    ; -----------------------------------------------
    ; Load & Copy All Blocks (Loop)
    ; -----------------------------------------------
    
    ; Setup Loop
    xor cx, cx          ; CX = Block Index (0 to 11)
    mov edi, 0x100000   ; Destination Address (Starting at 1MB)

.block_loop:
    cmp cx, 64          ; Max 64 blocks
    jge .copy_finished

    ; Get Block Number from Inode
    ; We must use the saved inode_ptr because DI is now corrupted by EDI usage
    mov si, [inode_ptr]
    
    mov bx, cx
    shl bx, 2           ; BX = CX * 4
    add bx, 37          ; BX = 37 + (CX * 4) (Offset to blocks[i]: 1+32+4=37)
    
    add si, bx          ; SI = Inode Addr + Offset
    mov eax, [si]       ; Get sector number
    
    ; Check for End of File (Block 0 means unused)
    cmp eax, 0
    je .copy_finished

    ; Read Block to Temp Buffer (0x8000)
    push cx             ; Save Loop Counter (CX)
    push edi            ; Save Dest Address (EDI)
    
    ; Re-initialize DAP every time just to be safe
    mov si, dap
    mov byte [dap], 0x10      ; Size = 16
    mov byte [dap+1], 0       ; Reserved = 0
    mov word [dap+2], 1       ; Sectors = 1
    mov word [dap.offset], 0x8000 ; Offset
    mov word [dap.segment], 0     ; Segment
    mov [dap.lba_low], eax    ; LBA Low
    mov dword [dap.lba_high], 0 ; LBA High

    mov dl, [BOOT_DRIVE]
    mov ah, 0x42
    int 0x13
    jc disk_error
    
    pop edi             ; Restore Dest Address
    pop cx              ; Restore Loop Counter

    ; Copy from 0x8000 to High Memory (Unreal Mode)
    push cx             ; Save Loop Counter
    push esi            ; Save SI
    
    mov cx, 512         ; Copy 512 bytes
    mov esi, 0x8000     ; Source
    
    ; dest is already in edi
    
.internal_copy_loop:
    mov al, [esi]
    mov [edi], al       ; 32-bit addr write (Unreal Mode magic)
    inc esi
    inc edi
    dec cx
    jnz .internal_copy_loop
    
    pop esi             ; Restore SI
    pop cx              ; Restore Loop Counter

    ; Next Block
    inc cx
    jmp .block_loop

.copy_finished:
    mov si, msg_load_done
    call print_string

    ; -----------------------------------------------
    ; Memory Detection (BIOS E820)
    ; -----------------------------------------------
    mov si, msg_detect_mem
    call print_string

    mov di, 0x8004          ; Store map entries starting at 0x8004
    xor ebx, ebx            ; EBX must be 0 to start
    xor bp, bp              ; Keep entry count in BP
    mov edx, 0x0534D4150    ; Place "SMAP" into edx
    mov eax, 0xe820
    mov [es:di + 20], dword 1 ; Force a valid ACPI 3.X entry
    mov ecx, 24             ; Ask for 24 bytes
    int 0x15
    jc .e820_failed         ; Carry set on first call means unsupported

    mov edx, 0x0534D4150    ; Some BIOS trash EDX
    cmp eax, edx            ; on success, eax must be 'SMAP'
    jne .e820_failed
    test ebx, ebx           ; If EBX=0, list is empty (useless)
    je .e820_failed
    jmp .e820_entry_loop

.e820_entry_loop:
    inc bp                  ; Got a valid entry
    add di, 24              ; Move to next entry slot
    
    test ebx, ebx           ; If EBX=0, we are done
    je .e820_done

    mov eax, 0xe820
    mov [es:di + 20], dword 1
    mov ecx, 24
    int 0x15
    jc .e820_done           ; Carry set means end of list usually
    mov edx, 0x0534D4150    ; Repair EDX
    jmp .e820_entry_loop

.e820_failed:
    mov si, msg_mem_err
    call print_string
    jmp $

.e820_done:
    mov [0x8000], bp        ; Store the entry count at 0x8000
    mov si, msg_mem_done
    call print_string

    ; -----------------------------------------------
    ; Switch to Protected Mode & Jump
    ; -----------------------------------------------
    
    cli                     ; Disable interrupts
    lgdt [gdt_descriptor]   ; Load GDT (Full 32-bit GDT)

    mov eax, cr0
    or eax, 1               ; Set PE bit
    mov cr0, eax

    jmp CODE_SEG:init_pm    ; Far jump to flush pipeline

    jmp $

; ----------------------------------------------------
; 32-bit Protected Mode Section
; ----------------------------------------------------
[bits 32]
init_pm:
    ; Update Segment Registers
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Setup Stack (at top of free memory, e.g., 0x90000)
    mov ebp, 0x90000
    mov esp, ebp

    ; JUMP TO KERNEL! (0x100000)
    call 0x100000

    jmp $

; ----------------------------------------------------
; Real Mode Helper Functions & Data
; ----------------------------------------------------
[bits 16]

; --- Helper Functions ---
strcmp:
    push ax
    push si
    push di
.loop:
    mov al, [si]
    mov ah, [di]
    cmp al, ah
    jne .not_equal
    test al, al
    jz .equal
    inc si
    inc di
    jmp .loop
.not_equal:
    or al, 1
    pop di
    pop si
    pop ax
    ret
.equal:
    xor al, al
    pop di
    pop si
    pop ax
    ret

disk_error:
    mov si, msg_disk_err
    call print_string
    jmp $
magic_error:
    mov si, msg_magic_err
    call print_string
    jmp $
print_string:
    mov ah, 0x0e
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

; --- Data ---
msg_stage2         db 'Stage 2: Entered.', 0x0d, 0x0a, 0
msg_unreal         db 'Stage 2: Unreal Mode Enabled.', 0x0d, 0x0a, 0
msg_read_sb        db 'Reading Superblock...', 0x0d, 0x0a, 0
msg_magic_ok       db 'Superblock Magic Valid!', 0x0d, 0x0a, 0
msg_kernel_found   db 'Kernel Found!', 0x0d, 0x0a, 0
msg_kernel_not_found db 'Kernel Not Found!', 0x0d, 0x0a, 0
msg_load_done      db 'All Kernel Blocks Loaded! Jump to PM...', 0x0d, 0x0a, 0
msg_copy_done      db 'First Block Copied to 1MB!', 0x0d, 0x0a, 0
msg_disk_err       db 'Disk Error!', 0x0d, 0x0a, 0
msg_magic_err      db 'Invalid Magic!', 0x0d, 0x0a, 0
msg_detect_mem     db 'Detecting Memory (E820)...', 0x0d, 0x0a, 0
msg_mem_done       db 'Memory Map Stored at 0x8000.', 0x0d, 0x0a, 0
msg_mem_err        db 'Memory Detection Failed!', 0x0d, 0x0a, 0

BOOT_DRIVE db 0
inode_table_lba dd 0
kernel_size dd 0
filename_target db 'kernel.bin', 0
inode_ptr dw 0

; GDT Definition for Protected Mode
align 4
gdt_start:
    dd 0, 0                 ; Null Descriptor

gdt_code:
    dw 0xFFFF               ; Limit (bits 0-15)
    dw 0x0000               ; Base (bits 0-15)
    db 0x00                 ; Base (bits 16-23)
    db 10011010b            ; Access (Present, Ring0, Code, Exec/Read)
    db 11001111b            ; Flags (4KB, 32-bit) + Limit (bits 16-19)
    db 0x00                 ; Base (bits 24-31)

gdt_data:
    dw 0xFFFF               ; Limit (bits 0-15)
    dw 0x0000               ; Base (bits 0-15)
    db 0x00                 ; Base (bits 16-23)
    db 10010010b            ; Access (Present, Ring0, Data, Read/Write)
    db 11001111b            ; Flags (4KB, 32-bit) + Limit (bits 16-19)
    db 0x00                 ; Base (bits 24-31)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

align 4
dap:
    db 0x10, 0x00
    dw 1
.offset:
    dw 0
.segment:
    dw 0
.lba_low:
    dd 0
.lba_high:
    dd 0
