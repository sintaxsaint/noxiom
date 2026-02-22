/* arch/arm64/uart_pl011.c — ARM PL011 UART driver
 *
 * Register offsets are part of the PL011 ARM IP specification —
 * they are the same on Pi 3, Pi 4, Pi 5, and any other board that
 * uses an ARM PL011 instance. The MMIO BASE address comes from the
 * Device Tree at runtime; it is never hard-coded in this driver.
 *
 * Baud rate: 115200 @ 48 MHz UART reference clock (default on Pi).
 *   IBRD = floor(48_000_000 / (16 * 115200)) = 26
 *   FBRD = round((48_000_000 / (16 * 115200) - 26) * 64) = 3
 */
#include "uart_pl011.h"
#include <stdint.h>

/* PL011 register offsets */
#define UARTDR    0x000     /* Data register (TX write / RX read) */
#define UARTFR    0x018     /* Flag register                       */
#define UARTIBRD  0x024     /* Integer baud rate divisor           */
#define UARTFBRD  0x028     /* Fractional baud rate divisor        */
#define UARTLCRH  0x02C     /* Line control register               */
#define UARTCR    0x030     /* Control register                    */
#define UARTIMSC  0x038     /* Interrupt mask set/clear            */

/* UARTFR bits */
#define FR_TXFF   (1u << 5) /* TX FIFO full  */
#define FR_RXFE   (1u << 4) /* RX FIFO empty */

/* UARTLCRH bits */
#define LCRH_FEN  (1u << 4) /* FIFO enable                 */
#define LCRH_8BIT (3u << 5) /* 8-bit word length           */

/* UARTCR bits */
#define CR_UARTEN (1u << 0) /* UART enable */
#define CR_TXE    (1u << 8) /* TX enable   */
#define CR_RXE    (1u << 9) /* RX enable   */

static volatile uint8_t *uart = 0;

static inline void mmio_w32(uint32_t off, uint32_t val) {
    *((volatile uint32_t *)(uart + off)) = val;
}

static inline uint32_t mmio_r32(uint32_t off) {
    return *((volatile uint32_t *)(uart + off));
}

void pl011_init(uint64_t base)
{
    uart = (volatile uint8_t *)base;

    /* Disable UART before configuration */
    mmio_w32(UARTCR, 0);

    /* Baud: 115200 @ 48 MHz */
    mmio_w32(UARTIBRD, 26);
    mmio_w32(UARTFBRD, 3);

    /* 8-bit, no parity, 1 stop bit, FIFOs enabled */
    mmio_w32(UARTLCRH, LCRH_8BIT | LCRH_FEN);

    /* Mask all interrupts (polled mode) */
    mmio_w32(UARTIMSC, 0);

    /* Enable UART, TX and RX */
    mmio_w32(UARTCR, CR_UARTEN | CR_TXE | CR_RXE);
}

void pl011_putchar(char c)
{
    if (!uart) return;
    while (mmio_r32(UARTFR) & FR_TXFF);    /* wait for TX FIFO space */
    mmio_w32(UARTDR, (uint32_t)(uint8_t)c);
}

char pl011_getchar(void)
{
    if (!uart) return '\0';
    while (mmio_r32(UARTFR) & FR_RXFE);    /* wait for RX data       */
    return (char)(mmio_r32(UARTDR) & 0xFF);
}
