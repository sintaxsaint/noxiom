/* arch/arm64/boot/entry.S — AArch64 kernel entry point
 *
 * Raspberry Pi GPU firmware loads kernel8.img at physical 0x80000.
 * Register state on entry (ARM64 boot protocol):
 *   x0 = 0 (reserved)
 *   x1 = DTB physical address  <-- MUST save immediately
 *   x2 = 0
 *   x3 = 0
 *   CPU is at EL2 (Pi 3/4) or EL1 (some firmware configs)
 *
 * Sequence:
 *   1. Save DTB address (x1) into callee-saved x20 before anything else
 *   2. Drop from EL2 → EL1 if needed (via HCR_EL2 + eret)
 *   3. Disable MMU, D-cache, I-cache (safe initial state)
 *   4. Set up SP_EL1 stack (64 KB .bss stack)
 *   5. Zero .bss section
 *   6. Load exception vector table into VBAR_EL1
 *   7. Store DTB address into g_dtb_addr global
 *   8. bl kmain — never returns
 */

.section .text.entry
.global _start

_start:
    /* ── Step 1: Save DTB address before any register use ────────── */
    /* x1 is overwritten by many operations; x20 is callee-saved.    */
    mov     x20, x1

    /* ── Step 2: Drop EL2 → EL1 if necessary ─────────────────────  */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    and     x0, x0, #3
    cmp     x0, #2
    beq     .Lfrom_el2
    b       .Lel1_setup             /* already at EL1                */

.Lfrom_el2:
    /* RW=1: EL1/EL0 run AArch64 */
    mov     x0, #(1 << 31)
    msr     hcr_el2, x0
    isb

    /* Jump target after eret */
    adr     x0, .Lel1_setup
    msr     elr_el2, x0

    /* SPSR_EL2: M[3:0]=0101 (EL1h), all DAIF masked */
    mov     x0, #0x3C5
    msr     spsr_el2, x0
    isb

    eret                            /* drops to EL1 at .Lel1_setup  */

.Lel1_setup:
    /* ── Step 3: Disable MMU, D-cache, I-cache ────────────────────  */
    mrs     x0, sctlr_el1
    bic     x0, x0, #(1 << 0)      /* M: MMU off                    */
    bic     x0, x0, #(1 << 2)      /* C: D-cache off                */
    bic     x0, x0, #(1 << 12)     /* I: I-cache off                */
    msr     sctlr_el1, x0
    isb

    /* ── Step 4: Set up kernel stack ──────────────────────────────  */
    adr     x0, stack_top
    mov     sp, x0

    /* ── Step 5: Zero .bss ─────────────────────────────────────────  */
    adr     x0, __bss_start
    adr     x1, __bss_end
.Lbss_zero:
    cmp     x0, x1
    bge     .Lbss_done
    str     xzr, [x0], #8
    b       .Lbss_zero
.Lbss_done:

    /* ── Step 6: Load exception vector table ──────────────────────  */
    adr     x0, vector_table
    msr     vbar_el1, x0
    isb

    /* ── Step 7: Store DTB address into C-visible global ──────────  */
    /* g_dtb_addr is a uint64_t defined in arch/arm64/hal_impl.c     */
    adr     x0, g_dtb_addr
    str     x20, [x0]

    /* ── Step 8: Call kmain() ──────────────────────────────────────  */
    bl      kmain

    /* kmain() should never return; halt if it does */
.Lhalt:
    msr     daifset, #0xf           /* disable all interrupts        */
    wfe
    b       .Lhalt

/* ── Kernel stack (64 KB) — in .bss so it's zeroed by the loop above ─── */
.section .bss
.align 4                            /* 16-byte aligned               */
.global stack_bottom
stack_bottom:
    .space  65536
.global stack_top
stack_top:

/* ── g_dtb_addr — DTB pointer from firmware, read by hal_hw_detect() ──── */
/* NOTE: This is also defined as extern in hal_impl.c (arm64).
 *       Defined here so entry.S can store into it before C starts.        */
.section .data
.align 3                            /* 8-byte aligned for 64-bit store */
.global g_dtb_addr
g_dtb_addr:
    .quad   0
