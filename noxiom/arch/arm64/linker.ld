/* arch/arm64/linker.ld — Noxiom ARM64 linker script
 *
 * Raspberry Pi GPU firmware loads kernel8.img at physical 0x80000.
 * _start must be at exactly byte 0 of kernel8.img (= physical 0x80000).
 *
 * Section ordering:
 *   .text.entry   — entry.S code (_start at 0x80000)
 *   .text.vectors — exception vector table (2 KB aligned for VBAR_EL1)
 *   .text         — all other code
 *   .rodata       — read-only data
 *   .data         — read-write data
 *   .bss          — zero-initialised data (stack lives here)
 */

ENTRY(_start)

SECTIONS
{
    /* GPU firmware loads the kernel here */
    . = 0x80000;

    /* Entry code: _start must be the very first byte */
    .text.entry ALIGN(8) :
    {
        *entry.o(.text.entry)
        *(.text.entry)
    }

    /* Exception vectors: VBAR_EL1 requires 2 KB alignment */
    .text.vectors ALIGN(2048) :
    {
        *(.text.vectors)
    }

    /* All other code */
    .text ALIGN(4096) :
    {
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
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        . = ALIGN(8);
        __bss_end = .;
    }
}
