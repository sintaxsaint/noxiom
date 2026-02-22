/* arch/arm64/hal_impl.c — HAL implementation for AArch64
 *
 * Implements every hal_*() function declared in kernel/src/hal.h.
 * Portable kernel code calls only hal_*() — this file routes those calls
 * to the ARM-specific drivers (PL011 UART, GIC, DTB parser, MIDR).
 *
 * On AArch64: serial == display.  Both map to the PL011 UART whose
 * MMIO base is discovered from the DTB at first use — never hard-coded.
 *
 * Fallback behaviour: if the DTB is missing or invalid (uart_base == 0),
 * all output functions silently do nothing.  The kernel still runs and
 * reaches the shell; the user just won't see any output until a UART is
 * wired correctly.
 */
#include "hal.h"          /* -Ikernel/src  */
#include "string.h"       /* -Ikernel/src  */
#include "dtb.h"          /* -Iarch/arm64  */
#include "uart_pl011.h"   /* -Iarch/arm64  */
#include "gic.h"          /* -Iarch/arm64  */
#include "midr.h"         /* -Iarch/arm64  */
#include <stdint.h>

/* DTB address written into .data by arch/arm64/boot/entry.S
 * before bl kmain — so it is valid before any C code runs. */
extern volatile uint64_t g_dtb_addr;

/* ── Lazy DTB parse — called once, result cached ────────────────────────── */
static dtb_result_t s_dtb;
static int          s_dtb_done = 0;

static void dtb_init(void)
{
    if (s_dtb_done)
        return;
    s_dtb_done = 1;
    dtb_parse((uint64_t)g_dtb_addr, &s_dtb);
}

/* ── Serial (early debug UART) ──────────────────────────────────────────── */
void hal_serial_init(void)
{
    dtb_init();
    if (s_dtb.uart_base)
        pl011_init(s_dtb.uart_base);
}

void hal_serial_putchar(char c)
{
    pl011_putchar(c);
}

void hal_serial_print(const char *s)
{
    while (*s)
        pl011_putchar(*s++);
}

/* ── Display (= UART on AArch64; VGA does not exist) ───────────────────── */
void hal_display_init(void)
{
    /* UART already initialised by hal_serial_init(); nothing extra needed. */
}

void hal_display_clear(void)
{
    /* ANSI VT100: erase entire screen, move cursor to home */
    hal_serial_print("\033[2J\033[H");
}

void hal_display_putchar(char c)
{
    pl011_putchar(c);
}

void hal_display_print(const char *s)
{
    hal_serial_print(s);
}

void hal_display_set_color(uint8_t c)
{
    /* No-op: UART targets ignore VGA colour attributes.
     * (Could emit ANSI colour codes in a future enhancement.) */
    (void)c;
}

/* ── Input (UART RX, blocking) ──────────────────────────────────────────── */
void hal_input_init(void)
{
    /* PL011 RX already enabled by pl011_init() inside hal_serial_init(). */
}

char hal_input_getchar(void)
{
    return pl011_getchar();
}

/* ── Interrupt controller (ARM GIC) ─────────────────────────────────────── */
void hal_intc_init(void)
{
    dtb_init();
    if (s_dtb.gic_dist_base && s_dtb.gic_cpu_base)
        gic_init(s_dtb.gic_dist_base, s_dtb.gic_cpu_base);
}

void hal_intc_unmask(uint32_t irq)
{
    gic_enable_irq(irq);
}

void hal_intc_send_eoi(uint32_t irq)
{
    gic_eoi(irq);
}

/* ── CPU init: VBAR_EL1 set in entry.S before kmain() — no-op here ─────── */
void hal_cpu_init(void)
{
}

/* ── Halt ────────────────────────────────────────────────────────────────── */
void hal_halt(void)
{
    /* Mask all interrupts then spin on WFE (saves power vs busy loop) */
    __asm__ volatile("msr daifset, #0xf" ::: "memory");
    for (;;)
        __asm__ volatile("wfe");
}

/* ── Hardware detection ─────────────────────────────────────────────────── */
void hal_hw_detect(void)
{
    dtb_init();   /* idempotent; s_dtb already populated by serial_init */

    g_hw_info.arch           = ARCH_ARM64;
    g_hw_info.ram_bytes      = s_dtb.ram_size;
    g_hw_info.cpu_cores      = s_dtb.cpu_count;
    g_hw_info.uart_base      = s_dtb.uart_base;
    g_hw_info.intc_dist_base = s_dtb.gic_dist_base;
    g_hw_info.intc_base      = s_dtb.gic_cpu_base;

    /* CPU model string from MIDR_EL1 (part number lookup, not board name) */
    midr_detect(g_hw_info.model_str, sizeof(g_hw_info.model_str));

    /* UART compatible string from DTB (e.g. "arm,pl011") */
    kstrncpy(g_hw_info.compat_str, s_dtb.uart_compat,
             sizeof(g_hw_info.compat_str) - 1);
}
