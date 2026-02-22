#pragma once
#include <stdint.h>

/* midr.h — MIDR_EL1 CPU identification for AArch64
 *
 * Reads the Main ID Register at EL1 to obtain the CPU implementer
 * and part number.  Matched against a table of ARM IP block part numbers
 * (e.g. 0xD08 = Cortex-A72) — NOT against board-specific model strings.
 *
 * The result goes into g_hw_info.model_str; no hard-coded board names
 * are ever used.
 */

/* Fill buf with a human-readable CPU model string (e.g. "ARM Cortex-A72").
 * buf must be at least len bytes; always NUL-terminated. */
void midr_detect(char *buf, uint32_t len);
