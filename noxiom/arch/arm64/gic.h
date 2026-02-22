#pragma once
#include <stdint.h>

/* GIC (Generic Interrupt Controller) driver — minimal subset.
 * Supports GIC-400 (Pi 3/4) and GIC-600 (Pi 5) at the basic interface.
 * Base addresses come from the DTB at runtime.
 *
 * Registers used:
 *   GICD (Distributor) — controls which IRQs are forwarded to CPUs
 *   GICC (CPU Interface) — per-CPU acknowledge / EOI
 */

void gic_init(uint64_t dist_base, uint64_t cpu_base);
void gic_enable_irq(uint32_t irq);
void gic_disable_irq(uint32_t irq);
/* Acknowledge an interrupt — returns the IRQ number */
uint32_t gic_ack(void);
/* Signal end-of-interrupt for the given IRQ number */
void gic_eoi(uint32_t irq);
