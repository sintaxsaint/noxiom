/* arch/arm64/gic.c — ARM Generic Interrupt Controller (GIC) driver
 *
 * Uses the GIC Architecture specification v2 CPU interface, which is
 * supported by GIC-400 (Cortex-A53/A72 platforms) and GIC-600 (A76).
 * Base addresses are supplied at runtime from the DTB.
 *
 * GICD (Distributor) registers:
 *   GICD_CTLR       +0x000  Distributor control
 *   GICD_ISENABLER  +0x100  Interrupt Set-Enable registers (32 IRQs each)
 *   GICD_ICENABLER  +0x180  Interrupt Clear-Enable registers
 *   GICD_IPRIORITYR +0x400  Interrupt Priority registers
 *   GICD_ITARGETSR  +0x800  Interrupt Processor Targets (which CPU gets it)
 *   GICD_ICFGR      +0xC00  Interrupt Configuration (edge/level)
 *
 * GICC (CPU Interface) registers:
 *   GICC_CTLR       +0x000  CPU interface control
 *   GICC_PMR        +0x004  Priority mask (0xFF = accept all priorities)
 *   GICC_IAR        +0x00C  Interrupt Acknowledge Register
 *   GICC_EOIR       +0x010  End of Interrupt Register
 */
#include "gic.h"
#include <stdint.h>

/* GICD register offsets */
#define GICD_CTLR       0x000
#define GICD_ISENABLER  0x100
#define GICD_ICENABLER  0x180
#define GICD_IPRIORITYR 0x400
#define GICD_ITARGETSR  0x800
#define GICD_ICFGR      0xC00

/* GICC register offsets */
#define GICC_CTLR       0x000
#define GICC_PMR        0x004
#define GICC_IAR        0x00C
#define GICC_EOIR       0x010

static volatile uint8_t *gicd = 0;
static volatile uint8_t *gicc = 0;

static void gicd_w32(uint32_t off, uint32_t val) {
    *((volatile uint32_t *)(gicd + off)) = val;
}
static void gicc_w32(uint32_t off, uint32_t val) {
    *((volatile uint32_t *)(gicc + off)) = val;
}
static uint32_t gicc_r32(uint32_t off) {
    return *((volatile uint32_t *)(gicc + off));
}

void gic_init(uint64_t dist_base, uint64_t cpu_base)
{
    gicd = (volatile uint8_t *)dist_base;
    gicc = (volatile uint8_t *)cpu_base;

    /* Enable distributor */
    gicd_w32(GICD_CTLR, 1);

    /* Set all interrupt priorities to 0xA0 (middle priority) */
    for (uint32_t i = 0; i < 256; i += 4)
        gicd_w32(GICD_IPRIORITYR + i, 0xA0A0A0A0);

    /* Route all SPIs to CPU 0 */
    for (uint32_t i = 32; i < 256; i += 4)
        gicd_w32(GICD_ITARGETSR + i, 0x01010101);

    /* Disable all interrupts initially */
    for (uint32_t i = 0; i < 256; i += 32)
        gicd_w32(GICD_ICENABLER + (i / 8), 0xFFFFFFFF);

    /* Accept all priority levels (0xFF = lowest threshold = accept all) */
    gicc_w32(GICC_PMR, 0xFF);

    /* Enable CPU interface */
    gicc_w32(GICC_CTLR, 1);
}

void gic_enable_irq(uint32_t irq)
{
    if (!gicd) return;
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    gicd_w32(GICD_ISENABLER + reg * 4, (1u << bit));
}

void gic_disable_irq(uint32_t irq)
{
    if (!gicd) return;
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    gicd_w32(GICD_ICENABLER + reg * 4, (1u << bit));
}

uint32_t gic_ack(void)
{
    if (!gicc) return 1023;     /* 1023 = spurious IRQ */
    return gicc_r32(GICC_IAR) & 0x3FF;
}

void gic_eoi(uint32_t irq)
{
    if (!gicc) return;
    gicc_w32(GICC_EOIR, irq);
}
