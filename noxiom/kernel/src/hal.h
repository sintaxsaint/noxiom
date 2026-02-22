#pragma once
/* hal.h — Noxiom Hardware Abstraction Layer
 *
 * Arch-neutral interface for all portable kernel code.
 * Each architecture provides:
 *   arch/<arch>/hal_impl.c  — implements every function declared here
 *
 * Portable code MUST include only this header (not vga.h, serial.h, etc.)
 */
#include <stdint.h>
#include <stddef.h>
#include "hal_hw_info.h"

/* ── Color constants ──────────────────────────────────────────────────────
 * VGA-compatible encoding. On UART-only targets (arm64) the color argument
 * to hal_display_set_color() is silently ignored.
 * Pack with: HAL_COLOR(foreground, background)
 */
#define HAL_COLOR_BLACK         0
#define HAL_COLOR_BLUE          1
#define HAL_COLOR_GREEN         2
#define HAL_COLOR_CYAN          3
#define HAL_COLOR_RED           4
#define HAL_COLOR_MAGENTA       5
#define HAL_COLOR_BROWN         6
#define HAL_COLOR_LIGHT_GREY    7
#define HAL_COLOR_DARK_GREY     8
#define HAL_COLOR_LIGHT_BLUE    9
#define HAL_COLOR_LIGHT_GREEN   10
#define HAL_COLOR_LIGHT_CYAN    11
#define HAL_COLOR_LIGHT_RED     12
#define HAL_COLOR_LIGHT_MAGENTA 13
#define HAL_COLOR_YELLOW        14
#define HAL_COLOR_WHITE         15

#define HAL_COLOR(fg, bg)  ((uint8_t)((uint8_t)(bg) << 4 | (uint8_t)(fg)))

/* ── Serial (early boot debug — available before display init) ────────── */
void hal_serial_init(void);
void hal_serial_putchar(char c);
void hal_serial_print(const char *s);

/* ── Display (text console) ───────────────────────────────────────────── *
 * x86_64: VGA text mode 80×25 at 0xB8000                                 *
 * arm64:  PL011 UART (ANSI "\033[2J\033[H" for clear, colors ignored)    */
void hal_display_init(void);
void hal_display_clear(void);
void hal_display_putchar(char c);
void hal_display_print(const char *s);
void hal_display_set_color(uint8_t color);  /* no-op on UART-only targets */

/* ── Input ────────────────────────────────────────────────────────────── */
void hal_input_init(void);
char hal_input_getchar(void);               /* blocks until char available */

/* ── Interrupt controller ─────────────────────────────────────────────── *
 * x86_64: 8259 PIC      arm64: ARM GIC                                   */
void hal_intc_init(void);
void hal_intc_unmask(uint32_t irq);
void hal_intc_send_eoi(uint32_t irq);

/* ── CPU-level init ───────────────────────────────────────────────────── *
 * x86_64: loads GDT + IDT                                                 *
 * arm64:  sets VBAR_EL1 (done in entry.S before kmain, this is a no-op) */
void hal_cpu_init(void);

/* ── Halt ─────────────────────────────────────────────────────────────── */
void hal_halt(void) __attribute__((noreturn));

/* ── Hardware detection ───────────────────────────────────────────────── *
 * hal_hw_detect(): arch-specific; fills g_hw_info fields                  *
 * hal_hw_score():  portable;     reads g_hw_info, returns tier            */
void      hal_hw_detect(void);
hw_tier_t hal_hw_score(void);

/* Global hardware info — written once at boot, then read-only */
extern hw_info_t g_hw_info;
