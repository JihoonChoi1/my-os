# --------------------------------------------------------
# Variable Definitions (Modify here when adding new files)
# --------------------------------------------------------
C_SOURCES = $(filter-out mkfs.c, $(wildcard *.c))
HEADERS = $(wildcard *.h)

# Automatically generate a list of .o files from .c files
OBJ_FILES = ${C_SOURCES:.c=.o}

# Compiler and Linker settings
CC = x86_64-elf-gcc
LD = x86_64-elf-ld

# C Compile flags (32-bit, freestanding, disable stack protection, etc.)
CFLAGS = -ffreestanding -m32 -g

# Linker flags (Binary format, 32-bit)
LDFLAGS = --oformat binary -m elf_i386 -T kernel.ld

# --------------------------------------------------------
# Main Target (Executed when 'make run' is called)
# --------------------------------------------------------
run: disk.img
	qemu-system-x86_64 -drive format=raw,file=disk.img

# --------------------------------------------------------
# OS Image Creation (Bootloader + Kernel + Zero Padding)
# --------------------------------------------------------
# OS Image Generation (Using mkfs tool)
# --------------------------------------------------------
disk.img: mkfs boot.bin loader.bin kernel.bin
	./mkfs

# Compile mkfs tool (Host)
mkfs: mkfs.c fs.h
	gcc -o mkfs mkfs.c

# --------------------------------------------------------
# Kernel Binary Creation (Linking)
# --------------------------------------------------------
# kernel.bin requires both assembly objects and C objects
# kernel.bin requires both assembly objects and C objects
kernel.bin: kernel_entry.o interrupt.o ${OBJ_FILES}
	${LD} -o $@ $^ ${LDFLAGS}

# --------------------------------------------------------
# Bootloader Compilation
# --------------------------------------------------------
# Stage 1: boot.bin (MBR)
boot.bin: boot.asm
	nasm $< -f bin -o $@

# Stage 2: loader.bin
loader.bin: loader.asm
	nasm $< -f bin -o $@

# --------------------------------------------------------
# Individual File Compilation Rules (Pattern Matching)
# --------------------------------------------------------

# Compile all .c files into .o files
%.o: %.c ${HEADERS}
	${CC} ${CFLAGS} -c $< -o $@

# Assemble all .asm files (for ELF format) into .o files
%.o: %.asm
	nasm $< -f elf -o $@


# Cleanup (make clean)
# --------------------------------------------------------
clean:
	rm -f *.bin *.o mkfs disk.img