/* arch/arm64/midr.c — CPU identification via MIDR_EL1
 *
 * KEY DESIGN RULE (same as dtb.c):
 *   We match on CPU part numbers defined by CPU IP block vendors,
 *   NOT on board-specific model strings ("raspberrypi,4-model-b" etc.).
 *   The same code runs on Pi 3/4/5 and any future AArch64 hardware.
 *
 * MIDR_EL1 bit layout:
 *   [31:24]  Implementer   (0x41 = ARM Ltd., 0x61 = Apple, 0x51 = Qualcomm…)
 *   [23:20]  Variant        (revision major)
 *   [19:16]  Architecture   (0xF = ARMv8 defined by ID regs)
 *   [15:4]   Part number    (identifies the CPU core)
 *   [3:0]    Revision       (minor revision)
 */
#include "midr.h"
#include "string.h"
#include <stdint.h>

/* ── CPU part number table ──────────────────────────────────────────────── */
typedef struct {
    uint32_t    implementer;  /* bits [31:24] */
    uint32_t    part;         /* bits [15:4]  */
    const char *name;
} cpu_entry_t;

static const cpu_entry_t cpu_table[] = {
    /* ARM Ltd. (implementer 0x41) */
    { 0x41, 0xD03, "ARM Cortex-A53"  },   /* Pi 3, Pi Zero 2 W           */
    { 0x41, 0xD04, "ARM Cortex-A35"  },
    { 0x41, 0xD05, "ARM Cortex-A55"  },
    { 0x41, 0xD07, "ARM Cortex-A57"  },
    { 0x41, 0xD08, "ARM Cortex-A72"  },   /* Pi 4                        */
    { 0x41, 0xD09, "ARM Cortex-A73"  },
    { 0x41, 0xD0A, "ARM Cortex-A75"  },
    { 0x41, 0xD0B, "ARM Cortex-A76"  },   /* Pi 5 big cores              */
    { 0x41, 0xD0C, "ARM Neoverse-N1" },
    { 0x41, 0xD0D, "ARM Cortex-A77"  },
    { 0x41, 0xD40, "ARM Neoverse-V1" },
    { 0x41, 0xD41, "ARM Cortex-A78"  },
    { 0x41, 0xD44, "ARM Cortex-X1"   },
    { 0x41, 0xD46, "ARM Cortex-A510" },   /* Pi 5 little cores           */
    { 0x41, 0xD47, "ARM Cortex-A710" },
    { 0x41, 0xD48, "ARM Cortex-X2"   },
    { 0x41, 0xD4B, "ARM Cortex-A78C" },
    { 0x41, 0xD4D, "ARM Cortex-A715" },
    { 0x41, 0xD4E, "ARM Cortex-X3"   },
    /* Apple Silicon (implementer 0x61) */
    { 0x61, 0x000, "Apple Silicon"   },   /* mask covers all variants    */
    /* Qualcomm (implementer 0x51) */
    { 0x51, 0x800, "Qualcomm Kryo"   },
    { 0x51, 0x801, "Qualcomm Kryo"   },
    { 0x51, 0x802, "Qualcomm Kryo"   },
    /* Broadcom (implementer 0x42) — used in Pi 1/2 */
    { 0x42, 0x00F, "Broadcom Cortex-A7" },
    /* Sentinel */
    { 0, 0, 0 },
};

/* ── midr_detect ─────────────────────────────────────────────────────────── */
void midr_detect(char *buf, uint32_t len)
{
    if (!buf || len == 0)
        return;

    uint64_t midr = 0;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));

    uint32_t implementer = (uint32_t)((midr >> 24) & 0xFF);
    uint32_t part        = (uint32_t)((midr >>  4) & 0xFFF);

    /* Walk the table — Apple Silicon: match on implementer only (part varies) */
    const cpu_entry_t *e = cpu_table;
    while (e->name) {
        int match = (e->implementer == implementer) &&
                    (e->implementer == 0x61 || e->part == part);
        if (match) {
            kstrncpy(buf, e->name, len);
            buf[len - 1] = '\0';
            return;
        }
        e++;
    }

    /* Unknown: format as "AArch64 CPU (impl=0xNN part=0xNNN)" */
    char tmp[64];
    char impl_s[8], part_s[8];
    kutoa(implementer, impl_s, 16);
    kutoa(part,        part_s, 16);
    kstrncpy(tmp, "AArch64 CPU (impl=0x", sizeof(tmp));
    uint32_t n = (uint32_t)kstrlen(tmp);
    kstrncpy(tmp + n, impl_s, sizeof(tmp) - n); n = (uint32_t)kstrlen(tmp);
    kstrncpy(tmp + n, " part=0x", sizeof(tmp) - n); n = (uint32_t)kstrlen(tmp);
    kstrncpy(tmp + n, part_s, sizeof(tmp) - n); n = (uint32_t)kstrlen(tmp);
    kstrncpy(tmp + n, ")", sizeof(tmp) - n);

    kstrncpy(buf, tmp, len);
    buf[len - 1] = '\0';
}
