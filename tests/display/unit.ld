ENTRY(test_entry)
MEMORY { ROM (rx) : ORIGIN = 0x10000, LENGTH = 1M
         RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 1M }
SECTIONS {
 .text : { *(.text*) *(.rodata*) } >ROM
 .data : { *(.data*) } >RAM
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } >RAM
 . = ALIGN(8); end = .; _end = .;
}
