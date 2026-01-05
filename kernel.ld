/* kernel.ld - Compact Version */
ENTRY(_start)

SECTIONS
{
    /* Kernel start address (Base address) */
    . = 0x1000;

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
}