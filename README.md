# MyOS: A 32-bit x86 Operating System

This project is a fully functional 32-bit x86 Operating System built entirely from scratch in C and Assembly.

## 🎥 Demo
Here is the OS booting up and running natively within a Docker container:

![Docker Run Demo](output.gif)

## ⚡ Quick Start
Experience the OS instantly on any platform (Windows, macOS, or Linux) with a single command. 
*(No need to mess with cross-compilers, local dependencies, or emulators!)*

```bash
docker run --platform linux/amd64 --rm -it cjihoon/my-os
```

## 🌟 Key Features

### 🚀 Booting: Custom Two-Stage Bootloader using Unreal Mode
*   **Stage 1 (MBR)**: The boot sequence starts when QEMU's default SeaBIOS loads the first sector of the hard disk into memory at `0x7C00`. Using standard BIOS disk interrupts (`int 0x13`), the MBR reads the Stage 2 bootloader from the disk and places it at `0x1000`.
*   **Stage 2 (Unreal Mode & SimpleFS)**: 
    * To bypass the 1MB memory restriction of 16-bit Real Mode, the bootloader enables the **A20 line**, loads a dummy Global Descriptor Table (GDT), and modifies segment registers to successfully enter **Unreal Mode**.
    * Operating in Unreal Mode, it uses the Disk Address Packet (DAP) structure and BIOS interrupts to parse the custom **SimpleFS** superblock directly from the disk image.
    * It locates the `kernel.bin` inode and begins loading the kernel into high memory (`0x100000` / 1MB). Because BIOS `int 0x13` cannot directly write to addresses above 1MB, the loader reads the file in 512-byte chunks into a lower memory buffer and dynamically copies them up to the 1MB mark.
    * Before finishing, it retrieves the system physical memory map via the **BIOS E820** service (saving it at `0x8000`) and officially transitions the CPU into **32-bit Protected Mode**.
*   **Higher Half Kernel Paging (`head.asm`)**: 
    * The first piece of `kernel.bin` executed is `head.asm`. Based on the linker script, the C kernel expects to be executed in the "Higher Half" (starting at virtual address `0xC0100000`).
    * To bridge the gap, `head.asm` manually constructs an initial **Page Directory** and **Page Table**, mapping the virtual address `0xC0100000` directly to the actual physical load address (`0x00100000`).
    * With the Memory Management Unit (MMU) safely configured and paging enabled, it effortlessly jumps into the high-half C kernel entry point.

### 🧠 Memory Management: PMM & VMM
*   **Physical Memory Management (PMM)**: 
    * During the bootloader stage, the BIOS `E820` system call is used to dynamically probe and retrieve the system's usable memory map. 
    * Upon entering the kernel, the PMM initializes by reserving the physical frames currently occupied by the kernel image, as well as the kernel stack (which is allocated at the very end of available memory).
    * The available RAM is then chunked into discrete **4KB physical frames**. A bitmap data structure manages these frames, ensuring lightning-fast `O(1)` allocation and deallocation speeds.
*   **Higher Half Kernel Isolation**: 
    * Guided by a custom linker script (`kernel.ld`), the kernel's virtual execution address is set to `0xC0100000` (above the 3GB boundary), while its actual physical Load Memory Address (LMA) remains at `0x100000` (1MB). This spatial separation securely isolates the supervisor code from the lower-memory User Space processes.
    * Prior to executing C code, `head.asm` prepares the MMU. To prevent the CPU from instantly faulting when paging is turned on, it temporarily creates an **Identity Mapping** for the first 4MB of RAM. 
    * Concurrently, it maps the 3GB virtual region down to the 1MB physical region. Once the instruction pointer safely jumps into the Higher Half virtual address space, the initial identity mapping is strictly unmapped to prevent user processes from maliciously accessing kernel space.
*   **Virtual Memory Management (VMM) & Copy-On-Write**: 
    * The OS utilizes standard x86 32-bit two-level paging (Page Directory and Page Tables). 
    * To maximize kernel efficiency and bypass complex address translations, a **Direct Mapping** strategy is employed. The entire 128MB of available physical RAM is linearly mapped directly onto the `0xC0000000` (3GB) virtual boundary. This allows the kernel to instantly manipulate any physical frame by simply applying a fixed memory offset.
    * **Copy-On-Write (COW) Forking**: When generating a new user process (e.g., executing applications like `thread_test.elf`), calling `fork()` traditionally requires duplicating the entire memory space. To bypass this massive overhead, the VMM uses a Copy-On-Write mechanism. It simply copies the parent's page directory, marks the shared pages as *Read-Only*, and increments their reference counters. A true deep copy (frame allocation) is deferred and only triggered via a Page Fault when a process attempts to mutate the shared memory data.

### ⏱️ Process & Scheduling
*   **Preemptive Round-Robin Multitasking**:
    * The Programmable Interval Timer (PIT) is configured to trigger a hardware timer interrupt at a rate of 50Hz.
    * The custom timer Interrupt Service Routine (ISR) is directly wired to the system scheduler (`schedule()`). This guarantees that the kernel physically preempts the CPU from the currently running process and yields execution authority to the next task in the ready queue.
    * Process Control Blocks (`process_t`) are managed using a Doubly Linked List. This data structure allows for instantaneous `O(1)` unlinking of terminated or blocked processes, keeping the Round-Robin algorithm extremely efficient.
*   **Software Context Switching in Assembly**:
    * To maximize performance, I deliberately bypassed Intel's Hardware Task State Segment (TSS) switching, which unconditionally forces the CPU to save and load numerous unnecessary registers, causing severe overhead.
    * Instead, a lightweight software context switch was written purely in Assembly. Upon a timer interrupt, the CPU hardware automatically pushes critical execution state registers (`EIP`, `CS`, `EFLAGS`, `ESP`, and `SS`). The custom Assembly interrupt handler then manually pushes only the essential remaining general-purpose and segment registers onto the active process's kernel stack.
    * The actual core of the context switch is ingeniously simple: the kernel merely swaps the current Stack Pointer (`ESP`) to point to the next scheduled process's kernel stack. Popping the preserved state from the new stack and issuing an `iret` instruction ensures a seamless and instantaneous execution transition between threads.
*   **Ring 3 User Space Isolation & System Calls**:
    * The OS natively enforces security boundaries: all core kernel logic executes with full privileges in **Ring 0**, while user applications run in the restricted **Ring 3** environment.
    * To securely drop a process into User Mode, the kernel simulates an interrupt return. By carefully pushing a forged interrupt stack frame—comprising the Ring 3 Data Segment (`SS`), User Stack Pointer (`ESP`), modified `EFLAGS`, Code Segment (`CS`), and the entry point of the user program (`EIP`)—a simple `iret` instruction gracefully transitions the CPU out of supervisor mode.
    * To ensure absolute protection of kernel resources, Ring 3 applications are strictly prohibited from directly accessing OS internals. Any request for OS functionality (e.g., executing a new program, memory allocation, or filesystem I/O) must be brokered safely via `int 0x80` software interrupts (System Calls).

### 🔒 Synchronization: Futex & Hybrid Architecture
*   **Hybrid Lock Architecture (Fast Path vs. Slow Path)**:
    * To maximize locking performance and strictly minimize syscall overhead, a **Futex (Fast Userspace Mutex)**-style hybrid architecture was implemented.
    * **The Fast Path (User Space)**: Under ideal conditions with no thread contention, locking and unlocking operations are aggressively optimized using GCC's built-in atomic intrinsics (e.g., `__sync_bool_compare_and_swap`, `__sync_sub_and_fetch`). These atomic instructions execute entirely within Ring 3 user space, acquiring or releasing the lock instantly without ever paying the heavy performance penalty of a kernel trap.
    * **The Slow Path (Kernel Space)**: Only when true contention occurs (i.e., another thread already holds the lock), does the lock mechanism gracefully fall back to the "Slow Path". It invokes a system call to register the thread into the kernel's wait queue, explicitly transitioning its state to `BLOCKED`.
    * By suspending blocked threads rather than relying on spinlocks, this Futex implementation completely eradicates wasteful CPU busy-waiting.
*   **High-Performance Concurrency Demonstration**:
    * To prove the robustness of this locking mechanism, a classic `producer_consumer` program is provided. By utilizing these hybrid locks, multiple threads safely and asynchronously share a buffer at extreme throughput, successfully preventing any race conditions or data corruption.

### 💾 File System: SimpleFS & Custom Disk Builder
*   **SimpleFS (Flat File System)**:
    * Instead of porting a heavy, complex traditional file system like FAT or ext2, a custom, highly lightweight flat file system known as **SimpleFS** was designed and implemented from scratch.
    * It operates on a streamlined architecture where a master Superblock is written at the absolute beginning of the disk. This Superblock explicitly records essential metadata for every enclosed file, including its exact filename, starting sector address, and file size.
*   **The `mkfs.c` Build Tool**:
    * To seamlessly generate bootable images offline, a dedicated host-side disk builder tool (`mkfs.c`) was written.
    * During the build process, `mkfs.c` reads the compiled bootloader, the C kernel body, and various user application ELF binaries (like the shell or test programs). It then sequentially packs them together into a single, contiguous raw disk image (`disk.img`), mathematically calculating and injecting the Superblock at sector zero.
*   **Runtime Execution**:
    * When the OS is running, the core SimpleFS subsystem interacts directly with the low-level ATA hard disk driver to parse the Superblock.
    * Whenever the user requests to execute a program via the shell, SimpleFS flawlessly resolves the string filepath into its exact physical disk sectors. The custom ELF loader then pulls those specific sectors into memory, seamlessly bringing user programs to life.

## 🔥 Technical Challenges

Building an operating system from scratch natively exposes you to the most raw, hardware-level bugs imaginable. Below are two highlighted debugging logs demonstrating deep root-cause analysis.

### 🐛 Debugging Log 1: User Mode Stack Alignment & Page Fault

**Date:** 2026-02-10  
**Module:** User Mode (`kernel/process.c`, `programs/shell.c`), GCC Toolchain  
**Severity:** Critical (User Program Crash)  

#### 1. Issue Description
- **Symptom:** A Page Fault occurred at `0xF01000` immediately after launching `shell.elf` (User Mode).
- **Observation:**
    - `0xF01000` is the start of an unmapped page (User Stack is mapped at `0xF00000`~`0xF00FFF`).
    - Using `while(1)` binary search confirmed the crash happens exactly at the entry of `shell.o`.
    - Even after changing the initial `ESP` to `0xF00FFF` (inside the valid page), the Page Fault persisted at the same address (`0xF01000`).
    - This was puzzling because `main()` in `shell.c` takes no arguments (`void main()`), so logically it shouldn't be reading from the stack.

#### 2. Debugging Process: Disassembling the Truth
To understand why the CPU was accessing memory I didn't tell it to, I analyzed the compiler-generated assembly.

1.  **Command:** `x86_64-elf-gcc -ffreestanding -m32 -c programs/shell.c -o shell.o`
2.  **Disassembly:** `x86_64-elf-objdump -d -M intel shell.o`
3.  **Result (The "Smoking Gun"):**
    ```nasm
    00000000 <main>:
       0:   8d 4c 24 04             lea    ecx,[esp+0x4]
       4:   83 e4 f0                and    esp,0xfffffff0
       7:   ff 71 fc                push   DWORD PTR [ecx-0x4]  <-- The crashing instruction
    ```

#### 3. Root Cause Analysis: GCC's Hidden "Prologue"
- **The "Why":** Modern x86 CPUs require the Stack Pointer (`ESP`) to be **16-byte aligned** to efficiently execute SIMD instructions (like `SSE`, `movaps`). If not aligned, these instructions trigger a General Protection Fault.
- **The "How":** GCC, by default, inserts a "Prologue" at the start of `main` to enforce this alignment.
    1.  `lea ecx, [esp+4]`: Calculates the address where the "Return Address" *should* be.
    2.  `and esp, -16`: Aligns the `ESP` downwards to the nearest 16-byte boundary.
    3.  `push [ecx-4]`: Copies the 4-byte Return Address from the old stack position to the new aligned position.
- **The Crash Mechanism:**
    - **My Setup:** I initialized `ESP` to `0xF00FFF`.
    - **The Instruction:** `push DWORD PTR [ecx-4]` tries to read 4 bytes from `0xF00FFF`.
    - **Physical Reality:**
        - Byte 1: `0xF00FFF` (Mapped, OK).
        - Byte 2: `0xF01000` (Unmapped, **Page Fault**).
        - Byte 3: `0xF01001` (Unmapped).
        - Byte 4: `0xF01002` (Unmapped).
    - **Conclusion:** The GCC prologue blindly assumed it could read a full 4-byte Return Address, causing a Cross-Page Read into invalid memory.
- **The "Ghost" Data:**
    - Since I enter User Mode via `IRET` (not `CALL`), there is no actual Return Address on the stack.
    - GCC, assuming `main` is called like any C function, attempts to preserve this non-existent return address.
    - It blindly reads from `[ESP]`, crashing into the unmapped page boundary.

#### 4. Resolution
- **Fix:** Adjusted initial User `ESP` to **`0xF00FFC`**.
- **Rationale:**
    - By setting `ESP` to `0xF00FFC` (4-byte aligned), the `push` instruction reads exactly 4 bytes (`0xF00FFC`~`0xF00FFF`).
    - These 4 bytes are fully within the mapped page, so the read succeeds (even if the value is garbage/zero).
    - The "ghost" return address (garbage) is safely pushed to the aligned stack, preserving GCC's assumption without crashing.
- **Outcome:** The shell launches successfully without crashing.

---

### 🐛 Debugging Log 2: COW Memory Corruption Due to Write Protection

**Date:** 2026-02-13  
**Module:** Memory Manager (VMM, COW), CPU Architecture (CR0 Register)  
**Severity:** Critical (Parent Process Memory Corruption)

#### 1. Issue Description
- **Symptom:** Executing `exec hello.elf` caused the child process to run successfully, but the system crashed with a Page Fault immediately after the child exited, failing to return to the shell.
- **Hypothesis:** Since the code finished but return failed, it seemed like a Scheduler or Context Switch bug.
- **Initial Attempt:** Using `while(1)` loops was ineffective because the crash happened during the delicate transition from Child to Parent.

#### 2. Debugging Process: Capturing the Crash State
1.  **QEMU Debugging:** Used `qemu-system-x86_64 -d int,cpu_reset -no-reboot` to freeze the CPU state at the exact moment of the crash.
2.  **Crash Analysis:**  
    ![Crash State Log](debug_logs/2026-02-13/2026-02-13_CrashScreen.png)
    - **CR2 (Fault Address):** `0x467C0BA1` (Invalid/Unmapped Address)
    - **CR3 (Page Directory):** `0x0027F000` (Verified to be the Shell's Page Directory)
    - **EIP (Instruction Pointer):** `0x00400299`  
    ![Register Values](debug_logs/2026-02-13/2026.02.13_register_values.png)

3.  **Address Verification:**
    - Confirmed via `objdump` that `0x400299` is a valid address within `shell.elf`.
    - However, the instruction at `0x400299` in `shell.elf` should NOT be accessing `0x467C0BA1`. There was no logical reason for this access in the source code.  
    ![Shell Disassembly](debug_logs/2026-02-13/2026-02-13_shell.elf.png)

#### 3. Root Cause Analysis: The Smoking Gun
- **Contradiction:** The code address (`EIP`) was valid for Shell, but the behavior (accessing `CR2`) was impossible for Shell's code.
- **Hypothesis:** The memory at `0x400299` must have been corrupted/overwritten by `hello.elf`.
- **Cross-Examination:**
    - I checked `hello.elf`'s disassembly at the same offset (`0x40029x`).  
    ![Hello Disassembly](debug_logs/2026-02-13/2026-02-13_hello.elf.png)
    - Found instruction sequence: `45 fc 01 8b 55 fc 8b 45`
    - **Decoded Instruction:** `add DWORD PTR [ebx+0x458bfc55], ecx`
    - **Manual Verification:**
        - `EBX` at crash: `0x00F00F4C`
        - Calculation: `0x00F00F4C` + `0x458BFC55` = **`0x467C0BA1`**
    - **Conclusion:** **Exact Match!** The Shell was executing `hello.elf`'s code logic inside its own address space.

- **The "Why":**
    - **COW Failure:** The Parent (Shell) and Child (Hello) shared the same physical frame (COW).
    - **Kernel Trap:** When `exec` loaded `hello.elf`, it wrote to this shared frame.
    - **Missing Protection:** The Kernel (Ring 0) ignored the Read-Only bit because the **Write Protect (WP)** bit in the **CR0** control register was **0** (Disabled).
    - **Result:** The Kernel overwrote the shared physical frame directly, destroying the Parent's code.

#### 4. Resolution
- **Fix:** Enabled the Write Protect (WP) bit in the CR0 register during kernel initialization.
- **Implementation (head.asm):**
    ```nasm
    mov eax, cr0
    or eax, 0x80010000 ; Set PG (Bit 31) and WP (Bit 16)
    mov cr0, eax
    ```
- **Outcome:** The CPU now strictly enforces Read-Only protection even for the Kernel. `exec` triggers a Page Fault as intended, activating the COW mechanism to allocate a new private frame for the Child. The Shell creates a child, waits, and resumes successfully.

> 💡 **Want to see more hardcore debugging stories?** Check out the full [`debug_log.md`](debug_log.md) file for more in-depth root cause analyses, including Triple Faults, PMM-VMM Aliasing, and Race Conditions!

## 🛠️ Build from Source

To ensure a seamless and reproducible build environment without polluting your local host machine with complex dependencies or cross-compilers, this project provides a fully automated multi-stage Docker setup.

**1. Clone the repository:**
```bash
git clone https://github.com/cjihoon/my-os.git
cd my-os
```

**2. Build the OS image gracefully within Docker:**
```bash
docker build --platform linux/amd64 -t my-os-builder .
```

**3. Run the compiled OS using QEMU natively inside the container:**
```bash
docker run --platform linux/amd64 --rm -it my-os-builder
```
