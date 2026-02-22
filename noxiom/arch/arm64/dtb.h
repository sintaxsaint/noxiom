#pragma once
#include <stdint.h>

/* Minimal FDT (Flattened Device Tree) parser.
 *
 * Finds ONLY what the kernel needs to boot:
 *   - UART base address (matched by compatible string, NOT board name)
 *   - GIC base addresses (matched by compatible string)
 *   - RAM size (from /memory node)
 *   - CPU count (from /cpus node)
 *
 * Compatible strings matched (ARM IP block names, not board names):
 *   UART:  "arm,pl011"  or  "brcm,bcm2835-aux-uart"
 *   GIC:   "arm,cortex-a15-gic"  or  "arm,gic-400"  or  "arm,gic-v3"
 *
 * If the DTB address is 0 or the magic is wrong, returns -1 and all
 * fields in dtb_result_t remain zero.  The kernel boots in FALLBACK mode.
 */

typedef struct {
    uint64_t uart_base;         /* MMIO base of first matching UART      */
    uint64_t gic_dist_base;     /* GIC distributor MMIO base             */
    uint64_t gic_cpu_base;      /* GIC CPU interface MMIO base           */
    uint64_t ram_base;          /* RAM physical base (usually 0)         */
    uint64_t ram_size;          /* Total RAM bytes                       */
    uint32_t cpu_count;         /* Number of CPU nodes in /cpus          */
    char     uart_compat[64];   /* Compatible string of matched UART     */
} dtb_result_t;

/* Parse the device tree at dtb_phys_addr.
 * Returns 0 on success, -1 on failure (bad magic or addr is 0).
 * On failure, *out is zeroed. */
int dtb_parse(uint64_t dtb_phys_addr, dtb_result_t *out);
