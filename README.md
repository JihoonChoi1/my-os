# My Custom OS

A 32-bit Protected Mode Operating System built from scratch in C and Assembly.

## 🌟 Key Features

### �️ Kernel & Core
- **32-bit Protected Mode**: Boots into full 32-bit mode with GDT enabled.
- **Preemptive Multitasking**: Round-Robin Scheduler with Context Switching.
- **Physical Memory Manager (PMM)**: Detection via BIOS E820 (In Progress).
- **Interrupt Handling**: Custom IDT & ISR for Hardware Interrupts (timer, keyboard).
- **System Timer**: PIT (8253) configured at 50Hz.

### � File System
- **SimpleFS**: Custom Flat File System.
- **mkfs Tool**: Custom tool to generate disk images with Bootloader, Kernel, and Files.
- **Multi-Stage Bootloader**:
  - **Stage 1 (MBR)**: Loads Stage 2 from reserved path.
  - **Stage 2 (Loader)**: Parses SimpleFS to find and load `kernel.bin` to 1MB.

### 🐚 Shell & Interaction
- **Interactive Shell**: Command-line interface with input buffering.
- **Built-in Commands**: `help`, `clear`, `ls`.
- **Keyboard Driver**: Full PS/2 Keyboard support with Scancode mapping.

## 🛠 Prerequisites (macOS)

```bash
brew install nasm
brew install qemu
brew install x86_64-elf-gcc
brew install x86_64-elf-binutils
```

### For Windows Users (Recommended: WSL2)
Use **WSL (Windows Subsystem for Linux)** (Ubuntu).

1. Install dependencies:
```bash
sudo apt update
sudo apt install nasm qemu-system-x86 build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo
```

2. **Important**: You MUST use the `x86_64-elf-gcc` cross-compiler.
   - Standard `sudo apt install gcc` will NOT work correctly because it targets Linux, not a bare-metal OS.
   - Please follow the [OSDev Wiki - GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler) guide to build the toolchain.
   - Alternatively, you can search for pre-built binaries (e.g., `brew` on Linux), but building from source is the gold standard.

## 🚀 Build & Run

The project uses a comprehensive `Makefile` to handle building the disk image.

### One-Command Run
To compile everything (Bootloader, Kernel, FS) and launch QEMU:

```bash
make run
```

### Clean Build Artifacts
```bash
make clean
```

## 📂 Project Structure

- **boot/**: Assembly bootloaders (`boot.asm`, `loader.asm`).
- **kernel/**: C Kernel source (`kernel.c`, `process.c`, `timer.c`, etc.).
- **drivers/**: Hardware drivers (`keyboard.c`, `ports.c`).
- **fs/**: File System tools (`mkfs.c`).
- **cpu/**: Interrupt handling (`idt.c`, `interrupt.asm`).

---
Start hacking the kernel by editing `kernel.c`! 🚀
