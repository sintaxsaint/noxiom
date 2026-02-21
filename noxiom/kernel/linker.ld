ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text ALIGN(4096) :
    {
        *entry.o(.text)     /* kernel entry must be first */
        *(.text*)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata*)
    }

    .data ALIGN(4096) :
    {
        *(.data*)
    }

    .bss ALIGN(4096) :
    {
        *(COMMON)
        *(.bss*)
    }
}
