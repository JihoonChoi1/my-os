# --------------------------------------------------------
# Variable Definitions
# --------------------------------------------------------
# Source Directories
DIRS = kernel cpu drivers mm fs
 
# C Sources (Find all .c files in DIRS and root)
C_SOURCES = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
HEADERS = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.h))
 
# Object Files
OBJ_FILES = ${C_SOURCES:.c=.o}
 
# Compiler and Linker settings
CC = x86_64-elf-gcc
LD = x86_64-elf-ld
 
# C Compile flags (Include all directories for header search)
CFLAGS = -ffreestanding -m32 -g $(foreach dir, $(DIRS), -I$(dir)) -mno-sse -mno-sse2 -mno-mmx
 
# Linker flags
LDFLAGS = --oformat binary -m elf_i386 -T kernel.ld
 
# --------------------------------------------------------
# Main Target
# --------------------------------------------------------
run: disk.img
	qemu-system-x86_64 -no-shutdown -serial stdio -drive format=raw,file=disk.img


 
# User Programs
PROGRAMS = programs/hello.elf programs/shell.elf programs/fork_cow.elf programs/thread_test.elf programs/producer_consumer.elf

# --------------------------------------------------------
# OS Image Creation
# --------------------------------------------------------
disk.img: mkfs boot.bin loader.bin kernel.bin $(PROGRAMS)
	./mkfs

# Compile User Programs (ELF)
programs/%.elf: programs/%.c programs/lib.c programs/linker.ld
	$(CC) -ffreestanding -nostdlib -m32 -g -Wl,-m,elf_i386 -T programs/linker.ld $< programs/lib.c -o $@ -mno-sse -mno-sse2 -mno-mmx
 
# Compile mkfs tool (Host) - Needs to find fs.h
mkfs: tools/mkfs.c fs/fs.h
	gcc -m64 -I fs -o $@ tools/mkfs.c
 
# --------------------------------------------------------
# Kernel Binary Creation
# --------------------------------------------------------
# Explicitly list assembly objects here
ASM_OBJS = kernel/context_switch.o

# kernel.bin requires assembly objects, C objects, and extra ASM objects
kernel.bin: kernel/head.o cpu/interrupt.o ${OBJ_FILES} ${ASM_OBJS}
	${LD} -o $@ $^ ${LDFLAGS}
 
# --------------------------------------------------------
# Bootloader Compilation
# --------------------------------------------------------
boot.bin: boot/boot.asm
	nasm $< -f bin -o $@
 
loader.bin: boot/loader.asm
	nasm $< -f bin -o $@
 
# --------------------------------------------------------
# Individual File Compilation Rules
# --------------------------------------------------------
 
# Compile C files (Generic rule for any directory)
%.o: %.c ${HEADERS}
	${CC} ${CFLAGS} -c $< -o $@
 
# Assemble Assembly files (Generic rule)
%.o: %.asm
	nasm $< -f elf -o $@


 
# --------------------------------------------------------
# Cleanup
# --------------------------------------------------------
clean:
	rm -f *.bin mkfs disk.img
	rm -f $(OBJ_FILES)
	rm -f kernel/head.o cpu/interrupt.o