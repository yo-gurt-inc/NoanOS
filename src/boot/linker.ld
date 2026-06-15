ENTRY(_start)

SECTIONS
{
    . = 0x100000;
    
    .text : {
        *(.text)
    }
    
    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data)
    }
    
    .bss : {
        bss_start = .;
        *(.bss)
        *(COMMON)
        bss_end = .;
    }
}
