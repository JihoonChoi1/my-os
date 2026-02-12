#include "process.h"
#include "ports.h"
#include "tss.h"
#include "vmm.h"
#include "pmm.h"

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

// Maximum processes
#define MAX_PROCESSES 10

// Process Array (Static TCBs)
static process_t processes[MAX_PROCESSES];
static uint32_t current_pid = 0;
static uint32_t process_count = 0;

extern void print_string(char *str);
extern void print_dec(int n);
extern void print_hex(uint32_t n);

// Initialize the process system
void init_multitasking()
{
    // 0. Mark all processes as TERMINATED initially
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        processes[i].state = PROCESS_TERMINATED;
        processes[i].next = 0;
        processes[i].parent_id = -1;
    }

    // 1. The Kernel itself is Process 0
    processes[0].id = 0;
    processes[0].parent_id = -1; // Kernel has no parent
    processes[0].state = PROCESS_RUNNING;

    // Get current CR3
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    processes[0].pd = (page_directory *)cr3;

    current_pid = 0;
    process_count = 1;

    print_string("Multitasking Initialized. Kernel is PID 0.\n");
}

// Setup a stack for a new task
// Emulates the stack state as if an interrupt occurred:
// PUSH EFLAGS -> PUSH CS -> PUSH EIP -> PUSHAD (Registers)
void create_task(void (*function)())
{
    if (process_count >= MAX_PROCESSES)
    {
        print_string("Error: Max Processes Reached.\n");
        return;
    }

    int pid = process_count;
    process_count++;

    processes[pid].id = pid;
    processes[pid].parent_id = current_pid; // Set Parent to Current Process (e.g., Kernel PID 0)
    processes[pid].next = 0;
    // processes[pid].pd = processes[0].pd;

    // Clone Page Directory (Deep Copy User Space, Share Kernel Space)
    // This gives the new task its own address space, allowing it to become a User Process later.
    extern uint32_t vmm_clone_directory(page_directory * src);
    processes[pid].pd = (page_directory *)vmm_clone_directory((page_directory *)P2V((uint32_t)processes[0].pd));
    // 1. Initialize Stack Pointer to the END of the array (Stack grows triggers downwards)
    uint32_t *stack_ptr = &processes[pid].stack[1023];

    // 2. Forge the Stack Content
    // Matching 'switch_task' which does: push ebx, esi, edi, ebp
    // So we push in order: RetAddr, EBX, ESI, EDI, EBP (Top)

    extern void task_wrapper();

    // A. Return Address (For 'ret') -> Jump to wrapper first!
    *stack_ptr-- = (uint32_t)task_wrapper;

    // B. Callee-Saved Registers (For 'pop ...')
    *stack_ptr-- = (uint32_t)function; // EBX = Function Pointer (used by wrapper)
    *stack_ptr-- = 0;                  // ESI
    *stack_ptr-- = 0;                  // EDI
    *stack_ptr-- = 0;                  // EBP (Top of Stack)

    // Note: We don't push EFLAGS here because 'switch_task' uses 'ret', not 'iret'.
    // 'task_wrapper' will do 'sti' to enable interrupts.

    // TO make it point to last pushed data
    processes[pid].esp = stack_ptr + 1;

    print_string("Created Task PID ");
    print_dec(pid);
    print_string("\n");

    processes[pid].state = PROCESS_READY;
}

// Helper stub defined in context_switch.asm
extern void fork_ret();

// Fork System Call
// Returns: Child PID to Parent, 0 to Child
// Args: regs - Pointer to the interrupt stack frame (Trap Frame) passed by syscall_handler
int sys_fork(registers_t *regs)
{
    // 1. Check Process Limit
    if (process_count >= MAX_PROCESSES)
    {
        return -1;
    }

    // 2. Identify Parent and Child PIDs
    uint32_t parent_pid = current_pid;
    uint32_t child_pid = process_count;
    process_count++;
    // while(1);
    //  3. Initialize Child Process Control Block (PCB)
    processes[child_pid].id = child_pid;
    processes[child_pid].parent_id = parent_pid;
    // processes[child_pid].state = PROCESS_READY;
    //  while(1);
    processes[child_pid].next = 0;
    // 4. Clone Address Space (Page Directory)
    // Deep Copy User Space, Share Kernel Space
    extern uint32_t vmm_clone_directory(page_directory * src);
    processes[child_pid].pd = (page_directory *)vmm_clone_directory((page_directory *)P2V((uint32_t)processes[parent_pid].pd));
    // print_hex((uint32_t)processes[child_pid].pd->m_entries[2]);
    //  5. Setup Child's Kernel Stack (The "Trap Frame" Method)
    //  Instead of copying the running stack of sys_fork (which is complex),
    //  we manually construct the stack so the child wakes up directly at 'fork_ret'.
    // while(1);
    //  A. Point to the top (high address) of the child's allocated kernel stack.
    //  Assuming stack size is 4KB (1024 uint32_t).
    uint32_t *child_stack_ptr = processes[child_pid].stack + 1024;
    // while(1);
    // -------------------------------------------------------------
    // Step 5-1: Push the Trap Frame (User Registers)
    // This restores the User Mode state (EIP, ESP, etc.) when child runs.
    // -------------------------------------------------------------

    // Move pointer down to make space for registers_t
    child_stack_ptr -= (sizeof(registers_t) / 4);
    registers_t *child_regs = (registers_t *)child_stack_ptr;
    // while(1);
    // Copy the Parent's register state (captured in syscall_handler) to Child's stack
    // Struct copy works in C (effectively memcpy)
    //*child_regs = *regs;
    uint32_t *src_ptr = (uint32_t *)regs;
    uint32_t *dst_ptr = (uint32_t *)child_regs;

    for (int i = 0; i < sizeof(registers_t) / 4; i++)
    {
        dst_ptr[i] = src_ptr[i];
    }
    // while(1);
    //  ★ CRITICAL: Force Child's return value (EAX) to 0
    child_regs->eax = 0;

    // -------------------------------------------------------------
    // Step 5-2: Forge Thread Context for switch_task
    // We simulate the stack frame that 'switch_task' expects to pop.
    // Usually: [EBX, ESI, EDI, EBP, RET_ADDR] (Order depends on your asm)
    // -------------------------------------------------------------

    // We need space for 4 saved registers + 1 return address = 5 slots
    child_stack_ptr -= 5;

    // Let's populate them directly as an array:
    // child_stack_ptr[0] -> EBX
    // child_stack_ptr[1] -> ESI
    // child_stack_ptr[2] -> EDI
    // child_stack_ptr[3] -> EBP
    // child_stack_ptr[4] -> Return Address (fork_ret)
    // while(1);
    child_stack_ptr[0] = 0; // EBX (Dummy)
    child_stack_ptr[1] = 0; // ESI (Dummy)
    child_stack_ptr[2] = 0; // EDI (Dummy)
    child_stack_ptr[3] = 0; // EBP (Dummy)
    // while(1);
    // ★ When switch_task executes 'ret', it will jump HERE:
    child_stack_ptr[4] = (uint32_t)fork_ret;
    // while(1);
    //  6. Save Child's ESP
    //  Store the final stack pointer into the PCB.
    //  The scheduler will load this into CPU ESP when switching to child.
    processes[child_pid].esp = (uint32_t *)child_stack_ptr;
    // 7. Return Child PID to Parent
    // This return only executes for the PARENT process.
    // while(1);
    processes[child_pid].state = PROCESS_READY;
    return child_pid;
}

// Simple Round-Robin Scheduler
void schedule()
{
    // If we only have the kernel (1 process) running, no need to switch
    if (process_count <= 1)
        return;

    // 1. Get current process info
    int prev_pid = current_pid;

    // 2. Select next process (Round-Robin with State Check)
    int next_pid = current_pid;
    int items_checked = 0;
    // while(1);
    do
    {
        next_pid++;
        if (next_pid >= process_count)
        {
            next_pid = 0;
        }
        items_checked++;

        // Safety Break: If we wrapped around and found nothing (shouldn't happen if Kernel is running)
        if (items_checked > process_count)
        {
            next_pid = 0; // Fallback to Kernel
            break;
        }
    } while (processes[next_pid].state != PROCESS_READY && processes[next_pid].state != PROCESS_RUNNING);

    current_pid = next_pid;

    // 3. Perform Context Switch ONLY if the task changed
    if (current_pid != prev_pid)
    {
        // if(current_pid == 2) {
        //     print_hex((uint32_t)processes[prev_pid].pd->m_entries[0]);
        //     print_string("\n");
        //     print_hex((uint32_t)processes[prev_pid].pd->m_entries[2]);
        //     print_string("\n");
        //     print_hex((uint32_t)processes[current_pid].pd->m_entries[0]);
        //     print_string("\n");
        //     print_hex((uint32_t)processes[current_pid].pd->m_entries[2]);
        //     print_string("\n");
        // }
        // Update TSS ESP0 to point to the TOP of the NEW task's kernel stack
        // This ensures that if an interrupt occurs in User Mode, ESP jumps here.
        extern void tss_set_stack(uint32_t kernel_esp);
        uint32_t new_kernel_stack = (uint32_t)(processes[current_pid].stack + 1024);
        tss_set_stack(new_kernel_stack);
        // Switch Page Directory (CR3) if different
        // This is crucial for process isolation (Step 4.3 Fork)
        if (processes[current_pid].pd != processes[prev_pid].pd)
        {
            __asm__ volatile("mov %0, %%cr3" ::"r"(processes[current_pid].pd));
        }

        // Switch from Prev -> Next
        // We pass:
        //  1. The new task's ESP (value)
        //  2. A pointer to the old task's ESP storage (so we can save the old ESP)
        switch_task(processes[current_pid].esp, &processes[prev_pid].esp);
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
    // Critical Section
    __asm__ volatile("cli");

    // 1. Record Exit Code
    processes[current_pid].exit_code = code;

    // 2. Set State to TERMINATED (Zombie)
    // The process will remain in memory
    // 3. Mark as Zombie (Parent needs to wait())
    processes[current_pid].state = PROCESS_TERMINATED;

    print_string("\n[Kernel] Process ");
    print_dec(current_pid);
    print_string(" exited with code ");
    print_dec(code);
    print_string(".\n");

    // End Critical Section (Though we won't return, schedule() re-enables via task switch)
    __asm__ volatile("sti");

    // 4. Yield CPU directly (Never returns)
    schedule();

    // Should unreachable
    while (1)
        ;
}

int sys_wait(int *status)
{
    // Check for any zombie children
    // If found: reap (freed conceptually) and return PID.
    // If running children exist: block (yield).
    // If no children: return -1.

    while (1)
    {
        int has_children = 0;

        for (int i = 0; i < process_count; i++)
        {
            if (processes[i].parent_id == current_pid)
            {
                has_children = 1;

                if (processes[i].state == PROCESS_TERMINATED)
                {
                    // Child is a Zombie! Reap it.
                    if (status)
                    {
                        *status = processes[i].exit_code;
                    }

                    // Mark as "Free" or "Reaped".
                    // reusing slots is complex without a free list.
                    // For now, we just leave it as TERMINATED but return its PID.
                    // To prevent re-waiting same process, we could set parent_id to -1 or similar.
                    processes[i].parent_id = -1; // Detach so we don't wait again

                    return processes[i].id;
                }
            }
        }

        if (!has_children)
        {
            return -1; // No children to wait for
        }

        // Children exist but are running. Yield.
        // In a better OS, we would set state=PROCESS_BLOCKED and wake up on exit.
        // But for simplicity: busy wait with yield.
        __asm__ volatile("sti; hlt"); // Wait for interrupt (timer) to reschedule
        // schedule() is called by timer ISR
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
