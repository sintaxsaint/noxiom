#pragma once
#include "hal_hw_info.h"

/* Detect x86_64 hardware properties via CPUID and CMOS.
 * Fills: arch, cpu_cores, ram_bytes, model_str.
 * All other hw_info_t fields are zeroed (not applicable on x86). */
void cpuid_detect(hw_info_t *info);
