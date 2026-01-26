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

# Debugging Log: `sys_fork` Implementation Issues

**Date:** 2026-01-26
**Module:** Kernel Process Management (`kernel/process.c`)
**Severity:** Critical (Kernel Panic / System Reset)

## 1. Issue: Race Condition in Process State Update
- **Symptom:** Executing `exec hello.elf` caused the screen to flicker rapidly and return to the shell immediately, indicating a sudden reset or crash.
- **Investigation:**
    - Used binary search with `while(1)` loops to isolate the crash point.
    - Identified that the crash happened immediately after `processes[child_pid].state = PROCESS_READY;`.
    - Realized that the Timer Interrupt fired right after this line.
- **Root Cause:**
    - The process state was set to `PROCESS_READY` *before* the child process's kernel stack (Trap Frame) was fully initialized.
    - The scheduler picked the uninitialized child process, leading to a context switch with invalid stack values.
- **Resolution:**
    - Moved the `PROCESS_READY` assignment to the very end of `sys_fork`, ensuring initialization is complete before scheduling.

## 2. Issue: Implicit memcpy in Struct Assignment
- **Symptom:** After fixing the race condition, the kernel crashed at a later point in `sys_fork`.
- **Investigation:**
    - Used binary search again and identified `*child_regs = *regs;` as the cause.
- **Root Cause:**
    - Struct assignment in C (`=`) implicitly calls `memcpy`.
    - Since the kernel is compiled in `freestanding` mode, `memcpy` is not linked, causing a jump to an invalid address.
- **Resolution:**
    - Replaced struct assignment with a manual member-wise copy using a loop:
    ```c
    uint32_t *src_ptr = (uint32_t*)regs;
    uint32_t *dst_ptr = (uint32_t*)child_regs;
    for (int i = 0; i < sizeof(registers_t) / 4; i++) {
        dst_ptr[i] = src_ptr[i];
    }
    ```
