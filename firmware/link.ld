/* CH32V203K8T6: Flash 64KB / SRAM 20KB */
ENTRY(_start)
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS
{
    .init : { KEEP(*(.init)) } > FLASH
    .text : { . = ALIGN(4); *(.text .text.*) *(.rodata .rodata.*) *(.srodata .srodata.*) . = ALIGN(4); } > FLASH
    _sidata = LOADADDR(.data);
    .data : {
        . = ALIGN(4); _sdata = .;
        *(.data .data.*)
        . = ALIGN(4);
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.*)
        . = ALIGN(4); _edata = .;
    } > RAM AT > FLASH
    .bss (NOLOAD) : { . = ALIGN(4); _sbss = .; *(.sbss .sbss.*) *(.bss .bss.*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.riscv.attributes) }
}
