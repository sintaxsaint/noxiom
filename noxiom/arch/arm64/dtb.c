/* arch/arm64/dtb.c — Minimal Flattened Device Tree (FDT) parser
 *
 * The DTB is produced by the Pi GPU firmware (or QEMU) at boot.
 * It uses big-endian byte order; our AArch64 CPU runs little-endian,
 * so every field must be byte-swapped before comparison.
 *
 * We walk the structure block looking for three kinds of nodes:
 *   /memory          → reg property gives RAM base + size
 *   /cpus/cpu@*      → count these to get cpu_count
 *   uart-compatible  → "arm,pl011" or "brcm,bcm2835-aux-uart"
 *   gic-compatible   → "arm,gic-400" etc.
 *
 * KEY DESIGN RULE:
 *   We match on IP-block compatible strings (defined by ARM or Broadcom),
 *   NOT on board-specific model strings ("raspberrypi,4-model-b" etc.).
 *   This means the same binary works on Pi 3/4/5 and any future hardware
 *   that uses the same IP blocks.
 */
#include "dtb.h"
#include "string.h"
#include <stdint.h>

/* FDT magic (big-endian) */
#define FDT_MAGIC       0xD00DFEED

/* FDT structure block tokens (big-endian) */
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009

/* FDT header (all fields big-endian) */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} fdt_header_t;

/* Byte-swap helpers (DTB is big-endian, CPU is little-endian) */
static inline uint32_t be32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0xFF000000u) >> 24);
}

static inline uint64_t be64(uint64_t v) {
    return ((uint64_t)be32((uint32_t)(v & 0xFFFFFFFFu)) << 32) |
            (uint64_t)be32((uint32_t)(v >> 32));
}

/* Read a 32-bit value from the structure block stream and advance ptr */
static inline uint32_t read_u32(const uint8_t **p) {
    uint32_t val;
    kmemcpy(&val, *p, 4);
    *p += 4;
    return be32(val);
}

/* Check if a compatible property value (space-separated list) contains
 * the target string. Returns 1 if found, 0 if not. */
static int compat_match(const char *prop_data, uint32_t len,
                         const char *target)
{
    uint32_t target_len = (uint32_t)kstrlen(target);
    const char *end = prop_data + len;
    const char *p = prop_data;

    /* Compatible is a NUL-separated list of strings */
    while (p < end) {
        uint32_t entry_len = (uint32_t)kstrlen(p);
        if (entry_len == target_len && kstrncmp(p, target, target_len) == 0)
            return 1;
        p += entry_len + 1;
    }
    return 0;
}

/* Parse a reg property: returns base address from first <address, size> pair.
 * addr_cells and size_cells say how many 32-bit words each component uses. */
static uint64_t parse_reg_base(const uint8_t *data, uint32_t len,
                                uint32_t addr_cells, uint32_t size_cells)
{
    (void)len;
    (void)size_cells;
    if (addr_cells == 2 && len >= 8) {
        uint32_t hi, lo;
        kmemcpy(&hi, data,     4);
        kmemcpy(&lo, data + 4, 4);
        return ((uint64_t)be32(hi) << 32) | be32(lo);
    }
    if (addr_cells == 1 && len >= 4) {
        uint32_t v;
        kmemcpy(&v, data, 4);
        return be32(v);
    }
    return 0;
}

static uint64_t parse_reg_size(const uint8_t *data, uint32_t len,
                                uint32_t addr_cells, uint32_t size_cells)
{
    uint32_t offset = addr_cells * 4;
    if (offset + size_cells * 4 > len) return 0;
    data += offset;
    if (size_cells == 2) {
        uint32_t hi, lo;
        kmemcpy(&hi, data,     4);
        kmemcpy(&lo, data + 4, 4);
        return ((uint64_t)be32(hi) << 32) | be32(lo);
    }
    if (size_cells == 1) {
        uint32_t v;
        kmemcpy(&v, data, 4);
        return be32(v);
    }
    return 0;
}

int dtb_parse(uint64_t dtb_phys_addr, dtb_result_t *out)
{
    kmemset(out, 0, sizeof(*out));

    if (!dtb_phys_addr)
        return -1;

    const uint8_t *base = (const uint8_t *)dtb_phys_addr;
    const fdt_header_t *hdr = (const fdt_header_t *)base;

    /* Validate magic */
    if (be32(hdr->magic) != FDT_MAGIC)
        return -1;

    const uint8_t *struct_block  = base + be32(hdr->off_dt_struct);
    const char    *strings_block = (const char *)(base + be32(hdr->off_dt_strings));

    /* Root-level #address-cells and #size-cells (default = 1) */
    uint32_t root_addr_cells = 1;
    uint32_t root_size_cells = 1;

    /* Walk state */
    const uint8_t *p = struct_block;
    int depth = 0;
    char cur_node[64] = "";

    /* Flags to track what we found at the current node */
    int    in_memory   = 0;
    int    in_cpus     = 0;
    int    in_cpu      = 0;
    int    in_uart     = 0;
    int    in_gic      = 0;
    char   cur_compat[256] = "";
    uint32_t cur_compat_len = 0;
    uint8_t  cur_reg_data[64];
    uint32_t cur_reg_len = 0;
    int    has_reg = 0;

    for (;;) {
        /* Align to 4 bytes */
        while (((uintptr_t)p & 3) != 0) p++;

        uint32_t token = read_u32(&p);

        if (token == FDT_END)
            break;

        if (token == FDT_NOP)
            continue;

        if (token == FDT_BEGIN_NODE) {
            /* Node name is a NUL-terminated string */
            const char *name = (const char *)p;
            p += kstrlen(name) + 1;

            /* Detect node types */
            in_memory = (depth == 1 && kstrncmp(name, "memory", 6) == 0);
            in_cpus   = (depth == 1 && kstrncmp(name, "cpus",   4) == 0);
            in_cpu    = (in_cpus && depth == 2 && kstrncmp(name, "cpu@", 4) == 0);

            if (in_cpu)
                out->cpu_count++;

            kstrncpy(cur_node, name, sizeof(cur_node) - 1);
            cur_compat_len = 0;
            cur_reg_len    = 0;
            has_reg        = 0;
            in_uart        = 0;
            in_gic         = 0;
            depth++;
            continue;
        }

        if (token == FDT_END_NODE) {
            /* Process accumulated properties for this node */
            if (in_memory && has_reg) {
                out->ram_base = parse_reg_base(cur_reg_data, cur_reg_len,
                                               root_addr_cells, root_size_cells);
                out->ram_size = parse_reg_size(cur_reg_data, cur_reg_len,
                                               root_addr_cells, root_size_cells);
            }
            if (in_uart && has_reg && !out->uart_base) {
                out->uart_base = parse_reg_base(cur_reg_data, cur_reg_len,
                                                root_addr_cells, root_size_cells);
                kstrncpy(out->uart_compat, cur_compat,
                         sizeof(out->uart_compat) - 1);
            }
            if (in_gic && has_reg && !out->gic_dist_base) {
                out->gic_dist_base = parse_reg_base(cur_reg_data, cur_reg_len,
                                                    root_addr_cells, root_size_cells);
                /* GIC CPU interface is second region: skip addr+size of first */
                uint32_t skip = (root_addr_cells + root_size_cells) * 4;
                if (cur_reg_len >= skip * 2) {
                    out->gic_cpu_base = parse_reg_base(
                        cur_reg_data + skip, cur_reg_len - skip,
                        root_addr_cells, root_size_cells);
                }
            }

            in_memory = 0;
            in_cpus   = (depth > 2) ? in_cpus : 0;
            in_cpu    = 0;
            in_uart   = 0;
            in_gic    = 0;
            depth--;
            continue;
        }

        if (token == FDT_PROP) {
            uint32_t prop_len     = read_u32(&p);
            uint32_t name_offset  = read_u32(&p);
            const char *prop_name = strings_block + name_offset;
            const uint8_t *prop_data = p;
            p += prop_len;

            /* Process interesting properties */
            if (kstrcmp(prop_name, "compatible") == 0) {
                cur_compat_len = prop_len < sizeof(cur_compat) ?
                                 prop_len : (uint32_t)sizeof(cur_compat) - 1;
                kmemcpy(cur_compat, prop_data, cur_compat_len);

                /* Check for UART compatible strings (ARM IP block names) */
                if (compat_match((const char *)prop_data, prop_len, "arm,pl011") ||
                    compat_match((const char *)prop_data, prop_len, "brcm,bcm2835-aux-uart"))
                    in_uart = 1;

                /* Check for GIC compatible strings */
                if (compat_match((const char *)prop_data, prop_len, "arm,cortex-a15-gic") ||
                    compat_match((const char *)prop_data, prop_len, "arm,gic-400") ||
                    compat_match((const char *)prop_data, prop_len, "arm,gic-v3"))
                    in_gic = 1;

                /* Root node address/size cells from compatible check */
            } else if (kstrcmp(prop_name, "#address-cells") == 0 && depth == 1) {
                uint32_t v; kmemcpy(&v, prop_data, 4);
                root_addr_cells = be32(v);
            } else if (kstrcmp(prop_name, "#size-cells") == 0 && depth == 1) {
                uint32_t v; kmemcpy(&v, prop_data, 4);
                root_size_cells = be32(v);
            } else if (kstrcmp(prop_name, "reg") == 0) {
                has_reg = 1;
                cur_reg_len = prop_len < sizeof(cur_reg_data) ?
                              prop_len : (uint32_t)sizeof(cur_reg_data);
                kmemcpy(cur_reg_data, prop_data, cur_reg_len);
            }
            continue;
        }

        /* Unknown token — stop parsing */
        break;
    }

    return 0;
}
