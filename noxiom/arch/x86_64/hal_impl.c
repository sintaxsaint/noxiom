/* arch/x86_64/hal_impl.c — HAL implementation for x86_64
 *
 * Wraps all x86-specific drivers into the arch-neutral HAL interface.
 * Portable code calls hal_*() — this file routes those to the real drivers.
 */
#include "hal.h"
#include "vga.h"
#include "serial_x86.h"
#include "keyboard_x86.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "cpuid.h"

/* ── Serial ──────────────────────────────────────────────────────── */
void hal_serial_init(void)           { serial_init(); }
void hal_serial_putchar(char c)      { serial_putchar(c); }
void hal_serial_print(const char *s) { serial_print(s); }

/* ── Display (VGA text mode) ─────────────────────────────────────── */
void hal_display_init(void)           { vga_init(); }
void hal_display_clear(void)          { vga_clear(); }
void hal_display_putchar(char c)      { vga_putchar(c); }
void hal_display_print(const char *s) { vga_print(s); }
void hal_display_set_color(uint8_t c) { vga_set_color(c); }

/* ── Input (PS/2 keyboard via IRQ1) ─────────────────────────────── */
void hal_input_init(void)  { keyboard_init(); }
char hal_input_getchar(void) { return keyboard_getchar(); }

/* ── Interrupt controller (8259 PIC) ────────────────────────────── */
void hal_intc_init(void)             { pic_init(); }
void hal_intc_unmask(uint32_t irq)   { pic_unmask((uint8_t)irq); }
void hal_intc_send_eoi(uint32_t irq) { pic_send_eoi((uint8_t)irq); }

/* ── CPU init (GDT + IDT) ───────────────────────────────────────── */
void hal_cpu_init(void) {
    gdt_init();
    idt_init();
}

/* ── Halt ────────────────────────────────────────────────────────── */
void hal_halt(void) {
    __asm__ volatile ("cli");
    for (;;)
        __asm__ volatile ("hlt");
}

/* ── Hardware detection ─────────────────────────────────────────── */
void hal_hw_detect(void) {
    cpuid_detect(&g_hw_info);
}
