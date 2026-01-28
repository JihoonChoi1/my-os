# Debugging Log: `simplefs.c` Kernel Crash

**Date:** 2026-01-20
**Module:** File System (`fs/simplefs.c`)
**Severity:** Critical (Kernel Panic/Hang)

## 1. Issue Description
- **Symptom:** The OS would hang or crash silently when attempting to execute a file (`exec hello.elf`).
- **Observation:** The shell command `exec` was issued, but no output appeared, and the system became unresponsive.

## 2. Debugging Process (Binary Search)
To isolate the exact line causing the crash, I inserted infinite loops (`while(1);`) at strategic points in the code.

1.  **Hypothesis 1:** Crash during ELF loading (`elf_load`).
    - *Result:* The code reached `elf_load` but crashed earlier in `fs_find_file`.
2.  **Hypothesis 2:** Crash inside `fs_find_file`.
    - *Test A:* Loop inserted *before* `*out_inode = *current_inode;`.
        - -> **Reached.** (System hung at the expected loop).
    - *Test B:* Loop inserted *after* `*out_inode = *current_inode;`.
        - -> **Never Reached.** (System crashed before hitting the loop).
    - **Conclusion:** The crash occurs exactly at the struct assignment line.

## 3. Root Cause Analysis
- **Code:** `*out_inode = *current_inode;`
- **Context:** `sfs_inode` is defined with `__attribute__((packed))`.
- **Mechanism:**
    - Packed structures may not be aligned to 4-byte boundaries.
    - When assigning a large packed structure (256 bytes), GCC tries to ensure safe copying by generating a call to `memcpy`.
    - **Problem:** Our OS kernel is *freestanding* and does not have a linked standard C library (`libc`), so `memcpy` is undefined.
    - **Outcome:** The CPU attempted to jump to a non-existent function or executed garbage code, leading to a Page Fault or Hang.

## 4. Resolution
- **Fix:** Replaced the implicit struct assignment with a manual, byte-wise memory copy function.
- **Implementation:**
  ```c
  // Replaced: *out_inode = *current_inode;
  memory_copy((char*)current_inode, (char*)out_inode, sizeof(sfs_inode));
  ```
- **Verification:** After applying the fix and removing debug loops, the `exec` command successfully loaded and executed the user program.

---

# Debugging Log: Userland Shell & Syscall Issues

**Date:** 2026-01-21
**Module:** User Mode, VMM, Syscall, Keyboard Driver
**Severity:** Blocking (Shell Unusable)

## 1. Issue: Page Fault immediately after loading Shell
- **Symptom:** After `elf_load("shell.elf")` and jumping to User Mode, a Page Fault occurred at `0xF01000`.
- **Observation:** The VMM log showed `Mapped User Stack at 0xF00000` (4KB).
- **Root Cause:** The user stack pointer (`ESP`) was initialized to `0xF01000`. Accessing the stack (pushing return addresses or local variables) immediately crossed the boundary of the single 4KB page or touched the unmapped top address.
- **Resolution:**
    - Expanded User Stack mapping in `mm/vmm.c` from 1 page (4KB) to 4 pages (16KB, `0xF00000` - `0xF04000`).

## 2. Issue: Garbled Output ("S S")
- **Symptom:** Shell tried to print "Welcome..." but only "S S" appeared.
- **Root Cause:** System Call ABI mismatch.
    - **User Side (`lib.c`):** Passed arguments in a custom order or incomplete registers.
    - **Kernel Side (`syscall.c`):** Expected arguments in standard registers (EBX, ECX, EDX) but received mismatching data.
- **Resolution:** Standardized to a Linux-like ABI.
    - **EAX:** Syscall Number
    - **EBX:** File Descriptor (1=stdout, 0=stdin)
    - **ECX:** Buffer Pointer
    - **EDX:** Length

## 3. Issue: Keyboard Input Not Echoing
- **Symptom:** Typing keys did not produce output, even though `syscall_read` was called.
- **Root Cause:** Incorrect register usage in key mapping. `syscall_read` was reading `EBX` for the buffer pointer, but `EBX` held the File Descriptor (0). The buffer pointer was in `ECX`.
- **Resolution:** Updated `syscall_read` to read the buffer pointer from `ECX`.

## 4. Issue: Keyboard Input Deadlock (Infinite Loop)
- **Symptom:** Even after fixing registers, `getchar()` would hang.
- **Context:**
    ```c
    // drivers/keyboard.c
    while (kb_head == kb_tail) {
        // ... waiting for interrupt ...
        // Stuck here forever!
    }
    ```
- **Root Cause:**
    1.  **Optimization:** The compiler might optimize the loop, assuming `kb_head` and `kb_tail` don't change.
        - -> **Fix:** Added `volatile` to the loop body (`__asm__ volatile("hlt")`) to prevent optimization and save power.
    2.  **Interrupt Masking (Critical):** The System Call IDT gate was set to `0xEE` (Interrupt Gate).
        - **Mechanism:** `0xEE` automatically clears the Interrupt Flag (`IF=0`) upon entry.
        - **Outcome:** While waiting in the `while` loop inside the syscall, hardware interrupts (Key Press) were ignored by the CPU. The handler never ran, so `kb_head` never updated.
- **Resolution:**
    - Changed IDT Gate `128` (Syscall) to `0xEF` (**Trap Gate**).
    - **Effect:** Trap Gates do *not* clear interrupts on entry. This allows hardware interrupts to preempt the syscall handler, updating the keyboard buffer and breaking the `while` loop.

---

# Debugging Log: sys_fork Implementation Issues

**Date:** 2026-01-26
**Module:** Process Manager (`kernel/process.c`), VMM
**Severity:** Critical (System Reset/Crash)

## 1. Issue: Race Condition in Process State Update
- **Symptom:** Executing `exec hello.elf` caused the screen to flicker rapidly and return to the shell immediately, indicating a crash or reset.
- **Investigation:** Used binary search with `while(1)` loops to pinpoint the crash location. Discovered that the crash occurred immediately after `processes[child_pid].state = PROCESS_READY;`. Verified that `child_pid` was valid.
- **Root Cause:** The process state was set to `PROCESS_READY` *before* the child process's kernel stack (Trap Frame) was fully initialized. The Timer Interrupt fired immediately after this line, causing the scheduler to pick the uninitialized child process. The CPU then attempted to switch context using garbage stack values, leading to a crash.
- **Resolution:** Moved the `PROCESS_READY` assignment to the very end of `sys_fork`, ensuring the child process is fully initialized before becoming eligible for scheduling.

## 2. Issue: Implicit memcpy in Struct Assignment
- **Symptom:** After fixing Issue 1, the kernel crashed again at a different location.
- **Investigation:** Again used `while(1)` binary search and identified the line `*child_regs = *regs;` as the culprit.
- **Root Cause:** In C, assigning one struct to another (e.g., `*child_regs = *regs`) often triggers an implicit call to `memcpy`. Since the kernel is compiled in **freestanding mode** (`-ffreestanding`), the standard library `memcpy` is not linked/available, causing a runtime error (undefined symbol or jump to invalid address).
- **Resolution:** Replaced the struct assignment with a manual loop to copy data element by element:
    ```c
    uint32_t *src_ptr = (uint32_t*)regs;
    uint32_t *dst_ptr = (uint32_t*)child_regs;
    for (int i = 0; i < sizeof(registers_t) / 4; i++) {
        dst_ptr[i] = src_ptr[i];
    }
    ```

---

# Debugging Log: Triple Fault & PMM-VMM Aliasing Analysis

**Date:** 2026-01-26 ~ 2026-01-27
**Module:** Memory Manager (PMM/VMM), Scheduler, Interrupts
**Severity:** Blocker (Triple Fault / Infinite Reboot)

## 1. Issue: Reboot upon `hello.elf` Return (ESP Issue)
- **Symptom:** `hello.elf` executed prints successfully but the system rebooted when returning to the shell (lost output).
- **Investigation:** Tracing revealed that the shell ran as PID 0 (Kernel Task role), so `init_multitasking` did not initialize its `esp`. Upon returning from syscall, the context switch loaded an invalid ESP, causing a Double/Triple Fault.
- **Resolution:** Modified `create_task()` to explicitly initialize the User Shell as PID 1 with a valid stack. PID 0 was delegated to be the Kernel Idle Task.

## 2. Issue: Persistent Triple Fault (CR2=0x10xxx)
- **Symptom:** Even after fixing the ESP issue, executing `exec` caused a Triple Fault (System Reboot).
- **Investigation:** 
    1.  **QEMU Debugging:** Used `-d int,cpu_reset -no-reboot` to capture the crash state.
        ![Triple Fault Log (QEMU)](triple_fault_log.jpg)
    2.  **State Analysis:** 
        - `CR3`: Pointed to the NEW Page Directory (Child Process).
        - `CR2`: `0x10xxxx` (Kernel Code Access).
        - **Meaning:** The Child Process seemingly lost access to the Kernel Code region (0-4MB mapping vanished), causing a Page Fault that escalated to Triple Fault.
    3.  **Debug Prints:** Inserted code in `vmm_clone_directory` to print `Src[0]` (Kernel Dir) vs `Dst[0]` (Child Dir).
        - `Src[0]`: Valid (`0x118xxx`).
        - `Dst[0]`: Valid (`0x118xxx`). **(Confusing: Logic seemed correct)**.
        - **Clue:** The `Clone Dir` Address itself was `0x523000` (~5MB).
- **Root Cause:** **PMM/VMM Aliasing**. The VMM initialization (`vmm_init`) statically mapped physical addresses **4MB-8MB** for User Text usage. However, it *did not allocate* these frames from the PMM.
    - **PMM:** Believed 5MB was "Free".
    - **VMM:** Believed 5MB was "User Space".
    - **Conflict:** PMM allocated 5MB (`0x523000`) for the **Page Directory**. Later, operations targeting the User Space (4MB+) overwrote this memory region, corrupting the Page Directory structure (specifically erasing the Kernel Mappings at Index 0).
- **Resolution:**
    1.  **PMM Reservation:** Updated `pmm_init` to reserve **0-16MB** as "Used". Only high memory (16MB+) is now allocated dynamically.
    2.  **Identity Mapping Extension:** Updated `vmm_init` to Identity Map up to 128MB, allowing the kernel to access high-memory frames.
    3.  **Outcome:** Triple Fault resolved. `fork()` and `exec()` now function stably.
