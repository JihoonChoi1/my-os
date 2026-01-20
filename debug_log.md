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
