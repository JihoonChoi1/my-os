/* kernel.ld - Compact Version */
ENTRY(_start)

SECTIONS
{
    /* Kernel start address (Base address) */
    . = 0x100000;

    /* .text: Executable code section */
    .text : {
        *(.text)
    }

    /* .rodata: Read-only data (constants, strings, etc.) */
    /* Placed immediately after the .text section */
    .rodata : {
        *(.rodata)
    }

    /* .data: Initialized global variables */
    .data : {
        *(.data)
    }

    /* .bss: Uninitialized variables */
    .bss : {
        *(.bss)
    }

    /* Define a symbol for the end of the kernel */
    _kernel_end = .;
}