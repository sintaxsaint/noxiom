/* arch/x86_64/cpuid.c — hardware detection for x86_64
 *
 * Uses CPUID to read:
 *   - CPU core count (topology leaf 0xB, or leaf 1 fallback)
 *   - CPU brand string (leaves 0x80000002-4)
 * Uses CMOS registers to estimate RAM (good enough for tier scoring;
 * a future improvement is to use the E820 map stored by stage2).
 */
#include "cpuid.h"
#include "io.h"
#include "string.h"
#include <stdint.h>

static inline void do_cpuid(uint32_t leaf, uint32_t subleaf,
                             uint32_t *eax, uint32_t *ebx,
                             uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

static uint32_t get_core_count(void)
{
    uint32_t eax, ebx, ecx, edx;

    /* Check maximum supported leaf */
    do_cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;

    if (max_leaf >= 0xB) {
        /* Leaf 0xB ECX=1: core-level topology — EBX = number of cores */
        do_cpuid(0xB, 1, &eax, &ebx, &ecx, &edx);
        uint32_t cores = ebx & 0xFFFF;
        if (cores > 0)
            return cores;
    }

    /* Fallback: leaf 1 EBX[23:16] = max logical processors per package */
    do_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    uint32_t logical = (ebx >> 16) & 0xFF;
    return logical ? logical : 1;
}

static void get_brand_string(char *out, size_t len)
{
    uint32_t eax, ebx, ecx, edx;

    do_cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000004) {
        kstrncpy(out, "x86_64 CPU", len - 1);
        out[len - 1] = '\0';
        return;
    }

    /* Brand string is 48 bytes across three CPUID leaves */
    uint32_t *p = (uint32_t *)out;
    do_cpuid(0x80000002, 0, &p[0],  &p[1],  &p[2],  &p[3]);
    do_cpuid(0x80000003, 0, &p[4],  &p[5],  &p[6],  &p[7]);
    do_cpuid(0x80000004, 0, &p[8],  &p[9],  &p[10], &p[11]);
    out[47] = '\0';

    /* Trim leading spaces (common in Intel brand strings) */
    char *s = out;
    while (*s == ' ')
        s++;
    if (s != out) {
        size_t slen = kstrlen(s);
        kmemcpy(out, s, slen + 1);
    }
}

static uint64_t get_ram_bytes(void)
{
    /* CMOS 0x30/0x31: extended memory above 1 MB in 1 KB units (up to 64 MB) */
    outb(0x70, 0x30);
    uint8_t lo = inb(0x71);
    outb(0x70, 0x31);
    uint8_t hi = inb(0x71);
    uint32_t kb_low = ((uint32_t)hi << 8) | lo;

    /* CMOS 0x34/0x35: extended memory above 16 MB in 64 KB units */
    outb(0x70, 0x34);
    uint8_t ext_lo = inb(0x71);
    outb(0x70, 0x35);
    uint8_t ext_hi = inb(0x71);
    uint32_t kb_ext = ((uint32_t)ext_hi << 8) | ext_lo;

    uint64_t total = (uint64_t)(1024 + kb_low) * 1024       /* bytes */
                   + (uint64_t)kb_ext * 64ULL * 1024;

    /* Floor at 128 MB so tier scoring never sees 0 on modern hardware */
    if (total < (uint64_t)128 * 1024 * 1024)
        total = (uint64_t)128 * 1024 * 1024;

    return total;
}

void cpuid_detect(hw_info_t *info)
{
    info->arch           = ARCH_X86_64;
    info->cpu_cores      = get_core_count();
    info->ram_bytes      = get_ram_bytes();
    info->uart_base      = 0;   /* x86 uses ISA port I/O, not MMIO */
    info->intc_base      = 0;
    info->intc_dist_base = 0;
    info->compat_str[0]  = '\0';
    get_brand_string(info->model_str, sizeof(info->model_str));
}
