[bits 32]

global switch_task

; void switch_task(uint32_t *next_esp, uint32_t **current_esp_ptr);
;
; Stack Layout (after 'call' and 'push' regs):
; [ESP + 24] current_esp_ptr (Argument 2)
; [ESP + 20] next_esp        (Argument 1)
; [ESP + 16] Return Address  (Pushed by Call)
; [ESP + 12] EBX             (Pushed by us)
; [ESP + 8]  ESI
; [ESP + 4]  EDI
; [ESP + 0]  EBP             <-- Current ESP

switch_task:
    ; 1. Save Callee-Saved Registers
    push ebx
    push esi
    push edi
    push ebp

    ; 2. Save Old ESP
    mov eax, [esp + 24]    ; Get 2nd argument (current_esp_ptr)
    mov [eax], esp         ; *current_esp_ptr = current_esp

    ; 3. Switch to New ESP
    mov esp, [esp + 20]    ; Get 1st argument (next_esp) and load into ESP

    ; --- CONTEXT SWITCH HAPPENS HERE ---

    ; 4. Restore Registers (from new stack)
    pop ebp
    pop edi
    pop esi
    pop ebx

    ; 5. Return to new task
    ret

; Used to start new tasks with interrupts enabled
; void task_wrapper();
; Expects: EBX = Function Address to call
global task_wrapper
task_wrapper:
    sti         ; Enable Interrupts (Critical for Preemption!)
    call ebx    ; Call the task function
    jmp $       ; Infinite loop if task returns (TODO: task_exit)

; Helper for fork()
; When the child process is first scheduled, it will "return" here.
; We simply set EAX = 0 (return value for child) and jump to the standardized interrupt exit routine.
extern isr_exit
global fork_ret
fork_ret:
    ; EAX = 0 (Success for child)
    mov eax, 0
    
    ; Jump to interrupt exit (restores registers from Trap Frame and IRET)
    jmp isr_exit
