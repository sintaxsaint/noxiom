#pragma once
#include <stdint.h>

/* PL011 UART driver — ARM IP block, same register layout on all Pi models.
 * MMIO base address is supplied at runtime (from DTB), never hard-coded. */

void pl011_init(uint64_t base);
void pl011_putchar(char c);
char pl011_getchar(void);           /* blocks until RX FIFO has data */
