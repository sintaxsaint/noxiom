/* kernel/src/hal_hw_detect.c — portable tier scoring
 *
 * hal_hw_detect() is implemented in arch/<arch>/hal_impl.c.
 * This file defines g_hw_info and implements hal_hw_score().
 */
#include "hal_hw_info.h"

/* Global hardware info — defined here, declared extern in hal.h */
hw_info_t g_hw_info;

hw_tier_t hal_hw_score(void) {
    uint32_t cores = g_hw_info.cpu_cores;
    uint64_t ram   = g_hw_info.ram_bytes;

    if (cores == 0 || ram == 0)
        return TIER_FALLBACK;

    if (cores >= 4 && ram >= (uint64_t)2 * 1024 * 1024 * 1024)
        return TIER_HIGH;

    if (cores >= 2 && ram >= (uint64_t)512 * 1024 * 1024)
        return TIER_MID;

    if (ram >= (uint64_t)128 * 1024 * 1024)
        return TIER_LOW;

    return TIER_FALLBACK;
}
