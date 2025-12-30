# My First OS Project

This project is a minimal 32-bit operating system built from scratch.
The bootloader is written in Assembly (NASM), and the kernel is written in C.

## 🛠 Prerequisites

You need the following tools to build and run this project.

### For macOS Users

Install the necessary toolchain using Homebrew:

```bash
brew install nasm
brew install qemu
brew install x86_64-elf-gcc
brew install x86_64-elf-binutils
```

## 🚀 How to Build & Run

Run the following commands in your terminal in order.

### 1. Build the Bootloader

```bash
nasm boot.asm -f bin -o boot.bin
```

### 2. Build the Kernel Entry Point

```bash
nasm kernel_entry.asm -f elf -o kernel_entry.o
```

### 3. Compile the C Kernel

```bash
x86_64-elf-gcc -ffreestanding -c kernel.c -o kernel.o -m32
```

### 4. Link the Kernel (Create Kernel Image)

_Note: macOS users must use `x86_64-elf-ld`._

```bash
x86_64-elf-ld -o kernel.bin -Ttext 0x1000 kernel_entry.o kernel.o --oformat binary -m elf_i386
```

### 5. Create OS Image

Combine the bootloader and the kernel into a single binary.

```bash
cat boot.bin kernel.bin > os-image.bin
```

### 6. Run (QEMU)

```bash
qemu-system-x86_64 -drive format=raw,file=os-image.bin
```

---

## ⚡️ Quick Run (One-Liner)

If you don't want to type commands one by one, copy and paste this block to rebuild and run everything at once:

```bash
rm -f *.bin *.o && \
nasm boot.asm -f bin -o boot.bin && \
nasm kernel_entry.asm -f elf -o kernel_entry.o && \
x86_64-elf-gcc -ffreestanding -c kernel.c -o kernel.o -m32 && \
x86_64-elf-ld -o kernel.bin -Ttext 0x1000 kernel_entry.o kernel.o --oformat binary -m elf_i386 && \
cat boot.bin kernel.bin > os-image.bin && \
qemu-system-x86_64 -drive format=raw,file=os-image.bin
```
