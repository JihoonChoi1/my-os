#include "process.h"
#include "ports.h"
#include "tss.h"
#include "vmm.h"
#include "pmm.h"
#include "sync.h"

// External function (Assembly) to switch context
// void switch_task(uint32_t *next_esp, uint32_t **current_esp_ptr);
extern void switch_task(uint32_t *next_esp, uint32_t **current_esp_ptr);

// Switch to User Mode (Ring 3)
// Arguments: entry_point - The address of the user program to execute
void enter_user_mode(uint32_t entry_point)
{
    // We need to set up the stack for IRET:
    // SS (User Data Segment with RPL=3)
    // ESP (User Stack Pointer)
    // EFLAGS (Interrupts enabled)
    // CS (User Code Segment with RPL=3)
    // EIP (Entry Point)

    // Segment Selectors:
    // Kernel Code: 0x08, Kernel Data: 0x10
    // User Code: 0x18 (Index 3) | 3 = 0x1B
    // User Data: 0x20 (Index 4) | 3 = 0x23

    // User Stack is mapped at 0xF00000 (15MB) - See vmm.c
    // Let's set ESP to 0xF00FFC (end of the mapped page approx)
    uint32_t user_stack = 0xF00FFC;

    __asm__ volatile(
        "mov $0x23, %%ax\n" // User Data Segment (Index 4 | 3)
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        "pushl $0x23\n" // SS (User Data Segment)
        "pushl %0\n"    // ESP (User Stack Pointer)
        "pushf\n"       // Push EFLAGS
        "pop %%eax\n"
        "or $0x200, %%eax\n" // Enable Interrupts (IF=1)
        "push %%eax\n"       // Push updated EFLAGS
        "pushl $0x1B\n"      // CS (User Code Segment (Index 3) | 3)
        "pushl %1\n"         // EIP (Entry Point)
        "iret\n"             // Interrupt Return -> Jumps to User Mode!
        :
        : "r"(user_stack), "r"(entry_point)
        : "eax", "memory");
}

#include "../mm/kheap.h"

// Process List (Doubly Linked)
static process_t *process_list = 0;
/* static process_t *current_process = 0; // Removed static for sync.c access */
process_t *current_process = 0;
static uint32_t next_pid = 0;

extern void print_string(char *str);
extern void print_dec(int n);
extern void print_hex(uint32_t n);
extern void memset(void *dest, int val, int len);

// Initialize the process system
void init_multitasking()
{
    // 1. Allocate Kernel Process (PID 0)
    process_list = (process_t*)kmalloc(sizeof(process_t));
    memset(process_list, 0, sizeof(process_t));

    process_list->id = 0;
    process_list->parent_id = -1; // Kernel has no parent
    process_list->state = PROCESS_RUNNING;

    // Get current CR3
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    process_list->pd = (page_directory *)cr3; // Physical Address

    process_list->next = 0;
    process_list->prev = 0;

    current_process = process_list;
    next_pid = 1;

    print_string("Multitasking Initialized. Kernel is PID 0.\n");
}

// Setup a stack for a new task
void create_task(void (*function)())
{
    process_t *new_task = (process_t*)kmalloc(sizeof(process_t));
    if (!new_task) {
        print_string("Error: OOM in create_task.\n");
        return;
    }
    memset(new_task, 0, sizeof(process_t));

    int pid = next_pid++;

    
    new_task->id = pid;
    new_task->parent_id = current_process->id;
    new_task->next = 0;
    new_task->prev = 0;

    // Clone Page Directory
    extern uint32_t vmm_clone_directory(page_directory * src);
    // P2V needed for access? vmm_clone reads src->m_entries.
    // current_process->pd IS physical. We need to pass Virtual.
    new_task->pd = (page_directory *)vmm_clone_directory((page_directory *)P2V((uint32_t)current_process->pd));
    
    // 1. Initialize Stack Pointer
    uint32_t *stack_ptr = &new_task->stack[1023];

    // 2. Forge the Stack Content
    extern void task_wrapper();

    // A. Return Address
    *stack_ptr-- = (uint32_t)task_wrapper;

    // B. Callee-Saved Registers
    *stack_ptr-- = (uint32_t)function; 
    *stack_ptr-- = 0;                  
    *stack_ptr-- = 0;                  
    *stack_ptr-- = 0;                  

    new_task->esp = stack_ptr + 1;

    print_string("Created Task PID ");
    print_dec(pid);
    print_string("\n");

    new_task->state = PROCESS_READY;

    // Append to linked list
    process_t *tail = process_list;
    while (tail->next) tail = tail->next;
    
    tail->next = new_task;
    new_task->prev = tail;
}

// Helper stub defined in context_switch.asm
extern void fork_ret();

// Fork System Call
// Returns: Child PID to Parent, 0 to Child
// Args: regs - Pointer to the interrupt stack frame (Trap Frame) passed by syscall_handler
int sys_fork(registers_t *regs)
{
    // 1. Allocate Child PCB
    process_t *child = (process_t*)kmalloc(sizeof(process_t));
    if (!child) return -1;
    memset(child, 0, sizeof(process_t));

    // 2. Identify IDs
    uint32_t parent_pid = current_process->id;
    uint32_t child_pid = next_pid++;

    // 3. Initialize Child
    child->id = child_pid;
    child->parent_id = parent_pid;
    child->next = 0;
    child->prev = 0;
    
    // 4. Clone Address Space
    extern uint32_t vmm_clone_directory(page_directory * src);
    child->pd = (page_directory *)vmm_clone_directory((page_directory *)P2V((uint32_t)current_process->pd));
    
    // 5. Setup Child's Kernel Stack
    uint32_t *child_stack_ptr = child->stack + 1024;
    
    // Move pointer down to make space for registers_t
    child_stack_ptr -= (sizeof(registers_t) / 4);
    registers_t *child_regs = (registers_t *)child_stack_ptr;
    
    // Copy the Parent's register state
    uint32_t *src_ptr = (uint32_t *)regs;
    uint32_t *dst_ptr = (uint32_t *)child_regs;

    for (int i = 0; i < sizeof(registers_t) / 4; i++)
    {
        dst_ptr[i] = src_ptr[i];
    }
    
    // ★ Force Child's return value (EAX) to 0
    child_regs->eax = 0;

    // Step 5-2: Forge Thread Context for switch_task
    child_stack_ptr -= 5;

    child_stack_ptr[0] = 0; // EBP
    child_stack_ptr[1] = 0; // EDI
    child_stack_ptr[2] = 0; // ESI
    child_stack_ptr[3] = 0; // EBX
    
    // ★ When switch_task executes 'ret', it will jump HERE:
    child_stack_ptr[4] = (uint32_t)fork_ret;
    
    // 6. Save Child's ESP
    child->esp = (uint32_t *)child_stack_ptr;
    
    child->state = PROCESS_READY;

    // 7. Append to List
    process_t *tail = process_list;
    while (tail->next) tail = tail->next;
    
    tail->next = child;
    child->prev = tail;

    return child_pid;
}

// sys_clone: Create a new kernel thread sharing memory space
// Input: regs->ebx = New Stack Pointer (allocated by user)
int sys_clone(registers_t *regs)
{
    // 1. Allocate process structure
    process_t *child = (process_t*)kmalloc(sizeof(process_t));
    if (!child) return -1;

    // 2. Setup IDs
    child->id = next_pid++;
    child->parent_id = current_process->id;
    child->state = PROCESS_READY;
    child->exit_code = 0;
    
    // 3. Shared Memory (CRITICAL DIFFERENCE FROM FORK)
    // Threads share the same Page Directory!
    extern irq_lock_t pd_ref_lock;
    irq_lock(&pd_ref_lock);
    child->pd = current_process->pd; 
    
    // INCREMENT Reference Count for the Shared Page Directory
    // Since pd is a virtual pointer in the kernel, we convert it to physical address to get the frame.
    extern void pmm_inc_ref(uint32_t addr);
    pmm_inc_ref(V2P((uint32_t)child->pd));
    irq_unlock(&pd_ref_lock);
    
    // 4. Setup Kernel Stack (Same logic as fork)
    uint32_t *stack_ptr = (uint32_t*)(child->stack + 1024);
    
    // Push Trap Frame (registers_t)
    stack_ptr -= sizeof(registers_t) / 4;
    registers_t *child_regs = (registers_t*)stack_ptr;
    *child_regs = *regs; // Copy registers from parent
    
    // Set Return Value for Child (0)
    child_regs->eax = 0;
    
    // Set New Stack Pointer (passed in EBX)
    if (regs->ebx != 0) {
        child_regs->useresp = regs->ebx; // Set User ESP in Trap Frame
        child_regs->ebp = 0;             // Clear EBP for clean stack trace
    }
    
    // Set Thread Entry Point (passed in ECX)
    if (regs->ecx != 0) {
        child_regs->eip = regs->ecx;
    }
    
    // 6. Manual Stack Setup for "fork_ret" (Kernel Thread Return)
    // We emulate what `switch_task` expects on the stack when it switches to this thread.
    // Order: [EBX, ESI, EDI, EBP, RET_ADDR] (Popped by switch_task)
    
    // Allocate 5 slots (20 bytes) on the stack
    stack_ptr -= 5;
    
    stack_ptr[0] = 0; // EBP
    stack_ptr[1] = 0; // EDI
    stack_ptr[2] = 0; // ESI
    stack_ptr[3] = 0; // EBX
    stack_ptr[4] = (uint32_t)fork_ret; // Return address
    
    child->esp = stack_ptr; // Save kernel stack pointer
    
    // 5. Add to process list
    process_t *tail = process_list;
    while (tail->next) tail = tail->next;
    tail->next = child;
    child->prev = tail;
    child->next = 0;
    
    return child->id; // Return Child PID (TID) to parent
}

// Block the current process and yield CPU
void block_process() {
    current_process->state = PROCESS_BLOCKED;
    schedule();
}

// Unblock a specific process (mark as READY)
void unblock_process(process_t *p) {
    if (p && p->state == PROCESS_BLOCKED) {
        p->state = PROCESS_READY;
    }
}

// Linked-List Round-Robin Scheduler
void schedule()
{
    // Atomic Schedule: Ensure no interrupts interrupt the scheduler itself
    __asm__ volatile("cli");

    if (!process_list || !process_list->next) return;

    // 1. Select next process
    process_t *next = current_process->next;
    if (!next) next = process_list; // Wrap around to head

    process_t *start_node = next;

    // Find first READY or RUNNING process
    while (next->state != PROCESS_READY && next->state != PROCESS_RUNNING) {
        next = next->next;
        if (!next) next = process_list;

        if (next == start_node) {
            return;
        }
    }

    // 2. Context Switch needed?
    if (next != current_process) {
        process_t *prev = current_process;
        current_process = next;

        // Update TSS ESP0 for User Mode interrupts
        extern void tss_set_stack(uint32_t kernel_esp);
        uint32_t new_kernel_stack = (uint32_t)(current_process->stack + 1024);
        tss_set_stack(new_kernel_stack);

        // Switch Page Directory (CR3) if different
        if (current_process->pd != prev->pd) {
            __asm__ volatile("mov %0, %%cr3" ::"r"(current_process->pd));
        }

        switch_task(current_process->esp, &prev->esp);
    }
}

// External declarations for sys_execve
extern uint32_t elf_load(char *filename);
extern void memset(void *dest, int val, int len);

int sys_execve(char *filename, char **argv, char **envp, registers_t *regs)
{
    // Critical Section: Disable Interrupts to prevent preemption during ELF load
    // The ATA Driver is Polling-based, so it works fine with interrupts disabled.
    __asm__ volatile("cli");

    // 1. Load the ELF file
    // Note: elf_load writes directly into the current Page Directory's User Space (0x400000)
    // It assumes the memory is already mapped (which it is, 4MB-8MB).
    uint32_t entry = elf_load(filename);

    if (!entry)
    {
        return -1; // Failed to load
    }

    // 2. Reset User Stack (0xF00000 - 0xF01000)
    // We clear the stack to ensure no data leaks from the previous process.
    // 0xF00000 is the bottom of the stack page range.

    // Ensure User Stack is Mapped
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    page_directory *current_pd = (page_directory *)P2V(cr3);

    if (!vmm_is_mapped(current_pd, 0xF00000))
    {
        uint32_t frame = pmm_alloc_block();
        if (!frame)
            return -1; // Stack Alloc Fail
        vmm_map_page_in_dir(current_pd, 0xF00000, frame, I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER);
    }

    memset((void *)0xF00000, 0, 4096);

    // 3. Update Trap Frame (regs) to "Return" to the New Program
    // When the syscall handler returns, it performs IRET using these values.
    regs->eip = entry;        // Jump to ELF Entry Point
    regs->useresp = 0xF00FFC; // Reset Stack Pointer to Top of Stack

    // Clear General Purpose Registers for a clean start
    regs->ecx = 0;
    regs->edx = 0;
    regs->ebx = 0;
    regs->ebp = 0; // Reset Base Pointer
    regs->esi = 0;
    regs->edi = 0;

    // EAX will be overwritten by the return value of this function (0),
    // effectively passing 0 to the new program.

    // End Critical Section
    __asm__ volatile("sti");

    return 0; // Success
}

// -------------------------------------------------
// Step 4.5: Exit & Wait Implementation
// -------------------------------------------------

void sys_exit(int code)
{
    __asm__ volatile("cli");

    current_process->exit_code = code;
    current_process->state = PROCESS_TERMINATED;

    print_string("\n[Kernel] Process ");
    print_dec(current_process->id);
    print_string(" exited with code ");
    print_dec(code);
    print_string(".\n");

    // Wake up parent if it is blocked waiting for us
    if (current_process->parent_id != -1) {
        process_t *node = process_list;
        while (node) {
            if (node->id == current_process->parent_id) {
                if (node->state == PROCESS_BLOCKED) {
                    node->state = PROCESS_READY;
                }
                break;
            }
            node = node->next;
        }
    }

    schedule();

    __asm__ volatile("sti");
    while (1) {
        __asm__ volatile("hlt");
    }
}

int sys_wait(int *status)
{
    while (1)
    {
        int has_children = 0;
        process_t *node = process_list;
        
        while (node) {
            if (node->parent_id == current_process->id) {
                has_children = 1;
                
                if (node->state == PROCESS_TERMINATED) {
                    // 1. Found Zombie Child
                    if (status) *status = node->exit_code;
                    int child_pid = node->id;
                    
                    // 2. Unlink from List
                    if (node->prev) node->prev->next = node->next;
                    if (node->next) node->next->prev = node->prev;

                    // 3. Free Resources
                    // A. Free Page Directory (and User Pages)
                    vmm_free_directory((page_directory*)P2V((uint32_t)node->pd));
                    
                    // B. Free PCB (and Kernel Stack inside it)
                    kfree(node);
                    
                    return child_pid;
                }
            }
            node = node->next;
        }

        if (!has_children)
        {
            return -1; // No children to wait for
        }

        // Children exist but are running.
        // Instead of busy waiting (HLT), mark itself as BLOCKED and yield.
        // It will be woken up when a child process calls sys_exit().
        block_process(); 
    }
}

// PID 1 Entry Point: Launches the Shell
void launch_shell()
{
    extern uint32_t elf_load(char *filename);

    print_string("[Kernel] Launching User Shell (PID 1)...\n");

    // 1. Load Shell
    // Note: This loads code into 0x400000.
    // Since PID 1 has its own Page Directory (cloned from kernel),
    // this write goes to PID 1's physical memory, not PID 0's.
    uint32_t shell_entry = elf_load("shell.elf");
    if (shell_entry)
    {
        // Ensure User Stack is Mapped for PID 1 (Shell)
        uint32_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        page_directory *current_pd = (page_directory *)P2V(cr3);

        if (!vmm_is_mapped(current_pd, 0xF00000))
        {
            uint32_t frame = pmm_alloc_block();
            if (frame)
            {
                vmm_map_page_in_dir(current_pd, 0xF00000, frame, I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER);
                memset((void *)0xF00000, 0, 4096);
            }
        }

        // 2. Jump to User Mode
        enter_user_mode(shell_entry);
    }
    else
    {
        print_string("[Kernel] Error: Could not load shell.elf\n");
        while (1)
            ;
    }
}
