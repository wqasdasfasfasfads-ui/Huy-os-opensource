ENTRY(_start)
SECTIONS {
    . = 0x10000;
    .text : { *(.text.entry) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    . = ALIGN(8);
    __bss_start = .;
    .bss : { *(.bss*) *(COMMON) }
    . = ALIGN(8);
    __bss_end = .;
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
