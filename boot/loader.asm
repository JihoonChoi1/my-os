[org 0x1000]    ; Tell the assembler that the code will be loaded at address 0x7c00
                ; This ensures all labels are calculated with 0x7c00 as the base.

start:
    mov [BOOT_DRIVE], dl    ; We save the boot drive number (dl) inherited from the MBR.

    mov si, msg_stage2      ; Set SI to the address of the message string.
    call print_string       ; Call the print_string function.

    ; Enable A20 Line

    ;"in destination, port": Read data from the hardware port into a register.
    in al, 0x92     ; Call port 0x92 (System Control Port A) and ask "What is your current status?
                    ; Save the answer in al
    or al, 2        ; do or operation with 0000 0010 (2) to set the 2nd bit (A20 enable)
    out 0x92, al    ; Write the modified value back to Port 0x92, enabling the A20 line.

    ; Enter Unreal Mode
    cli                     ; Disable interrupts
    lgdt [gdt_descriptor]   ; Load the Global Descriptor Table (GDT) into the CPU's GDTR register.
    mov eax, cr0    ; Move the value of the Control Register 0 (CR0) into the EAX register.
    or eax, 1       ; Set the 1st bit (PE: Protection Enable) of EAX.
    mov cr0, eax    ; Write the modified value back to CR0, enabling protected mode.
    jmp $+2         ; Jump to the next instruction (a short jump to itself).
                    ; A jump instruction forces the CPU to discard its pre-fetched 
                    ; instructions ("Flush Pipeline") and fetch them again under the new 32-bit rules. 
                    ; $+2 just jumps to the very next instruction.
    mov bx, 0x10    ; Set BX to 0x10 (the GDT data segment selector).
    mov ds, bx      ; Set the Data Segment (DS) register to 0x10.
    mov es, bx      ; Set the Extra Segment (ES) register to 0x10.
    mov eax, cr0    ; Move the value of the Control Register 0 (CR0) into the EAX register.
    and al, 0xFE    ; Clear the 1st bit (PE: Protection Enable) of EAX.
    mov cr0, eax    ; Write the modified value back to CR0, disabling protected mode.
                    ; CPU does not clear the hidden cache when switching back. 
                    ; The ds register still "thinks" it has a 4GB limit, even though we are in Real Mode. 
                    ; This allows us to use 32-bit offsets like [ds:0x100000] later.
    jmp $+2         ; Jump to the next instruction. Again for pipeline flush.
    xor ax, ax      ; Clear the AX register.
    mov ds, ax      ; Set the Data Segment (DS) register to 0x00.
                    ; The CPU interprets the value in ds differently depending on the mode.
                    ; Protected Mode: ds is a Selector (Table Index). 0x08 means "GDT Entry #1". 
                    ;                 Base address comes from the GDT (which is 0).
                    ; Real Mode: ds is a Segment Base. 0x0008 means "Memory Address 0x00080"(Multiplied by 16 for accessing 20-bit address).
    mov es, ax      ; Set the Extra Segment (ES) register to 0x00.
    sti             ; Enable interrupts

    mov si, msg_unreal
    call print_string

    ; Read Superblock (LBA 17) -> 0x2000
    mov si, dap                     ; Set si to point to the dap structure we defined at the bottom of the file.        
    mov word [dap.lba_low], 17      ; Set Target Sector
    mov word [dap.offset], 0x2000   ; Set Target Offset
    mov dl, [BOOT_DRIVE]            ; Load the boot drive ID we saved earlier into dl
    mov ah, 0x42                    ; Set command to 'Extended Read Sectors
    int 0x13                        ; Trigger BIOS Interrupt!
                                    ; BIOS reads the DAP at ds:si, goes to LBA 17, 
                                    ; reads 1 sector (defined in DAP default), and writes it to 0x2000
    jc disk_error                   ; If BIOS fails, it sets the Carry Flag (CF) to 1. 
                                    ; We catch errors using jc (Jump if Carry).

    mov si, msg_read_sb
    call print_string

    ; Verify Magic
    mov eax, [0x2000]   ; Load the magic number from 0x2000
    cmp eax, 0x12345678 ; Compare it with the expected value
    jne magic_error     ; If they are not equal, jump to magic_error

    mov si, msg_magic_ok
    call print_string

    ; Find "kernel.bin"
    mov eax, [0x200C]             ; Read inode table location from superblock.
                                  ; Expected value is 19. it reads 4 bytes since eax is 32-bit register.
    mov [inode_table_lba], eax    
    mov si, dap
    mov [dap.lba_low], eax        ; Set Target Sector to Inode Table LBA  
    mov word [dap.offset], 0x3000 ; Set Destination to 0x3000
    mov dl, [BOOT_DRIVE]          ; Load the boot drive ID we saved earlier into dl
    mov ah, 0x42                  ; Set command to 'Extended Read Sectors
    int 0x13                      ; Trigger BIOS Interrupt!
    jc disk_error                 ; If BIOS fails, it sets the Carry Flag (CF) to 1. 
                                  ; We catch errors using jc (Jump if Carry).

    mov di, 0x3000                ; Set DI to 0x3000 (Inode Table)
    mov cx, 5                     ; Set CX to 5 (Number of Inodes)
.find_loop:
    mov al, [di]                  ; Load 1st byte of inode (used flag) into AL
    cmp al, 1                     ; Check if the inode is used
    jne .next_inode               ; If not used, jump to .next_inode

    push di
    inc di
    mov si, filename_target       ; Set SI to filename_target
    call strcmp                   ; Compare filename_target with the name of the current inode
    pop di    
    je .found_kernel              ; If match (Zero Flag set), jump to success!

.next_inode:
    add di, 256                   ; Move DI to the next inode (Size of inode = 256 bytes)
    loop .find_loop               ; dec cx         
                                  ; cmp cx, 0      
                                  ; jne .find_loop   
    jmp .kernel_not_found         ; If CX is 0, jump to .kernel_not_found

.kernel_not_found:
    mov si, msg_kernel_not_found
    call print_string
    jmp $

.found_kernel:
    mov si, msg_kernel_found
    call print_string

    mov [inode_ptr], di          ; Save Inode Pointer because we need EDI for destination (and DI is lower 16-bit of EDI)

    ; -----------------------------------------------
    ; Load & Copy All Blocks (Loop)
    ; -----------------------------------------------
    
    ; Setup Loop
    xor cx, cx          ; CX = Block Index (0 to 11)
    mov edi, 0x100000   ; Destination Address (Starting at 1MB)

.block_loop:
    cmp cx, 64          ; Max 64 blocks
    jge .copy_finished

    mov si, [inode_ptr] ; Load address of the Kernel Inode
    
    mov bx, cx          ; Move Loop Counter (Block Index) to BX
    shl bx, 2           ; Multiply by 4 (Since each block ID is 4 bytes: uint32_t)
    add bx, 37          ; Add Offset to `blocks` array
                        ; Offset Math:
                        ; 1 byte (used)
                        ; + 32 bytes (filename)
                        ; + 4 bytes (size)
                        ; = 37 bytes.
    add si, bx          ; SI = Address of inode.blocks[cx]
    mov eax, [si]       ; Read the Sector Number (LBA) from memory
    
    ; Check for End of File (Block 0 means unused/end)
    cmp eax, 0
    je .copy_finished

    ; Read Block to Temp Buffer (0x8000)
    push cx             ; Save Loop Counter (CX) - BIOS destroys registers
    push edi            ; Save Dest Address (EDI)
    
    ; Re-initialize DAP every time just to be safe
    mov si, dap
    mov byte [dap], 0x10      ; Packet Size = 16 bytes (Size of DAP)
    mov byte [dap+1], 0       ; Reserved = 0
    mov word [dap+2], 1       ; Sector Count = 1 (Read 512 bytes)
    mov word [dap.offset], 0x8000 ; Set Destination to 0x8000
    mov word [dap.segment], 0     ; Buffer Segment (0x0000)
    mov [dap.lba_low], eax    ; LBA Low (The sector we calculated above)
    mov dword [dap.lba_high], 0 ; LBA High (0)

    mov dl, [BOOT_DRIVE]    ; Drive Number
    mov ah, 0x42            ; Extended Read
    int 0x13                ; Call BIOS
    jc disk_error           ; Check for error
    
    pop edi             ; Restore Dest Address (0x100000)
    mov cx, 512         ; Copy 512 bytes
    mov esi, 0x8000     ; Source
    
.internal_copy_loop:
    mov al, [esi]       ; Read 1 byte from Low Memory
    mov [edi], al       ; Write 1 byte to High Memory (Possible due to Unreal Mode!)
    inc esi             ; Next Source Address
     inc edi            ; Next Destination Address
    dec cx              ; Decrement Loop Counter
    jnz .internal_copy_loop
    
    pop cx              ; Restore Loop Counter

    ; Next Block
    inc cx              ; Increment Block Index
    jmp .block_loop

.copy_finished:
    mov si, msg_load_done
    call print_string

    ; -----------------------------------------------
    ; Memory Detection (BIOS E820)
    ; -----------------------------------------------
    mov si, msg_detect_mem
    call print_string

    mov di, 0x8004           ; Location to store the memory map
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
