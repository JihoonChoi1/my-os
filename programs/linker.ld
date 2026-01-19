ENTRY(main)

SECTIONS {
    /* User programs call loaded at 4MB (check VMM if this is mapped!) */
    /* Wait, VMM identity maps 0-4MB, and heap 8-12MB. 
       4MB is potentially unmapped or kernel code end?
       Kernel is at 1MB. 
       Let's stick to a safe address. 
       Actually, exec() will need to map this VA.
       Let's use 0x400000 (4MB) and ensure VMM maps it.
    */
    . = 0x400000;

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}
