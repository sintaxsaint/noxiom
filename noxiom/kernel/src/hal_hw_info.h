#pragma once
/* hal_hw_info.h — hardware info structs and tier definitions
 * Included by hal.h; also usable standalone by arch-specific code. */
#include <stdint.h>

typedef enum {
    ARCH_X86_64 = 0,
    ARCH_ARM64  = 1,
    ARCH_UNKNOWN
} hw_arch_t;

typedef enum {
    TIER_FALLBACK = 0,  /* unknown / detection failed — minimal safe config */
    TIER_LOW      = 1,  /* 1-2 cores, 128-512 MB RAM                        */
    TIER_MID      = 2,  /* 2-3 cores, 512 MB – 2 GB RAM                     */
    TIER_HIGH     = 3,  /* ≥4 cores AND ≥2 GB RAM                           */
} hw_tier_t;

typedef struct {
    hw_arch_t  arch;
    uint32_t   cpu_cores;       /* logical/physical core count               */
    uint64_t   ram_bytes;       /* total detectable RAM                      */
    char       model_str[128];  /* CPU model (CPUID brand string or MIDR)    */
    char       compat_str[128]; /* DTB compatible string (arm64 only)        */

    /* Peripheral MMIO bases — 0 means not present / not detected */
    uint64_t   uart_base;       /* UART MMIO (arm64: from DTB)               */
    uint64_t   intc_base;       /* GIC CPU interface (arm64: from DTB)       */
    uint64_t   intc_dist_base;  /* GIC distributor (arm64: from DTB)         */

    hw_tier_t  tier;            /* set by hal_hw_score() after hal_hw_detect */
} hw_info_t;
