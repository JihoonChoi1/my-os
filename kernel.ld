/* kernel.ld - Higher-Half Kernel Linker Script */
ENTRY(_start)

SECTIONS
{
    /* * [VMA: Virtual Memory Address]
     * The kernel's virtual address starts at 3GB (0xC0000000) + 1MB offset.
     * We use 0xC0100000 instead of 0xC0000000 because the first 1MB of 
     * physical RAM is reserved for BIOS, VGA MMIO, and real-mode structures.
     * By adding the 1MB offset to the 3GB base, we create a simple 1:1 
     * mapping relationship: Physical Address = Virtual Address - 0xC0000000.
     */
    . = 0xC0100000;

    /* * .text: Executable code section 
     * AT(0x100000) defines the [LMA: Load Memory Address].
     * This tells the bootloader to physically place the kernel at 1MB in RAM,
     * which is the first available 'clean' memory block for the kernel.
     */
    .text : AT(0x100000) {
        *(.text)
    }

    /* * .rodata: Read-only data (constants, string literals)
     * LMA is calculated relative to the 3GB base (0xC0000000).
     * This ensures that physically, this section follows immediately after .text.
     */
    .rodata : AT(ADDR(.rodata) - 0xC0000000) {
        *(.rodata)
    }

    /* * .data: Initialized global and static variables 
     * Same LMA calculation to maintain physical contiguity.
     */
    .data : AT(ADDR(.data) - 0xC0000000) {
        *(.data)
    }

    /* * .bss: Uninitialized variables (zero-filled by the kernel or bootloader) 
     */
    .bss : AT(ADDR(.bss) - 0xC0000000) {
        *(.bss)
    }

    /* * _kernel_end: A symbol used in the C/C++ code to mark the 
     * end of the kernel's memory footprint. 
     * Useful for initializing the Physical Memory Manager (PMM).
     */
    _kernel_end = .;
}