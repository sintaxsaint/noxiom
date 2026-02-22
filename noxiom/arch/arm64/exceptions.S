/* arch/arm64/exceptions.S — AArch64 exception vector table
 *
 * The vector table must be 2 KB aligned (VBAR_EL1 requirement).
 * Each entry is 128 bytes (32 instructions max).
 * We use a single branch instruction per entry to jump to handlers
 * defined below the table, where we can use as many instructions as needed.
 *
 * Table layout (offsets from VBAR_EL1):
 *   0x000  Synchronous    Current EL, SP_EL0
 *   0x080  IRQ            Current EL, SP_EL0
 *   0x100  FIQ            Current EL, SP_EL0
 *   0x180  SError         Current EL, SP_EL0
 *   0x200  Synchronous    Current EL, SP_EL1  ← kernel mode (we handle this)
 *   0x280  IRQ            Current EL, SP_EL1  ← kernel mode (we handle this)
 *   0x300  FIQ            Current EL, SP_EL1
 *   0x380  SError         Current EL, SP_EL1
 *   0x400  Synchronous    Lower EL, AArch64
 *   0x480  IRQ            Lower EL, AArch64
 *   0x500  FIQ            Lower EL, AArch64
 *   0x580  SError         Lower EL, AArch64
 *   0x600  Synchronous    Lower EL, AArch32
 *   0x680  IRQ            Lower EL, AArch32
 *   0x700  FIQ            Lower EL, AArch32
 *   0x780  SError         Lower EL, AArch32
 */

.section .text.vectors
.align 11               /* 2^11 = 2048 bytes */

.global vector_table
vector_table:

/* ── Current EL, SP_EL0 (kernel should never use SP_EL0) ─────────────── */
    b   arm64_exception_panic   /* 0x000 Sync  SP_EL0 */
    .align 7
    b   arm64_exception_panic   /* 0x080 IRQ   SP_EL0 */
    .align 7
    b   arm64_exception_panic   /* 0x100 FIQ   SP_EL0 */
    .align 7
    b   arm64_exception_panic   /* 0x180 SErr  SP_EL0 */
    .align 7

/* ── Current EL, SP_EL1 (kernel-mode exceptions — these matter) ────────── */
    b   el1_sync                /* 0x200 Sync  SP_EL1 */
    .align 7
    b   el1_irq                 /* 0x280 IRQ   SP_EL1 */
    .align 7
    b   arm64_exception_panic   /* 0x300 FIQ   SP_EL1 */
    .align 7
    b   arm64_exception_panic   /* 0x380 SErr  SP_EL1 */
    .align 7

/* ── Lower EL AArch64 ───────────────────────────────────────────────────── */
    b   arm64_exception_panic   /* 0x400 Sync  Lower AArch64 */
    .align 7
    b   arm64_exception_panic   /* 0x480 IRQ   Lower AArch64 */
    .align 7
    b   arm64_exception_panic   /* 0x500 FIQ   Lower AArch64 */
    .align 7
    b   arm64_exception_panic   /* 0x580 SErr  Lower AArch64 */
    .align 7

/* ── Lower EL AArch32 ───────────────────────────────────────────────────── */
    b   arm64_exception_panic   /* 0x600 Sync  Lower AArch32 */
    .align 7
    b   arm64_exception_panic   /* 0x680 IRQ   Lower AArch32 */
    .align 7
    b   arm64_exception_panic   /* 0x700 FIQ   Lower AArch32 */
    .align 7
    b   arm64_exception_panic   /* 0x780 SErr  Lower AArch32 */
    .align 7

/* ── Exception handlers (outside the table, no size limit) ─────────────── */

/* Save all general-purpose registers onto the stack.
 * We save x0-x30, ESR_EL1, ELR_EL1, SPSR_EL1 = 256 bytes total. */
.macro save_context
    sub     sp,  sp,  #256
    stp     x0,  x1,  [sp, #0]
    stp     x2,  x3,  [sp, #16]
    stp     x4,  x5,  [sp, #32]
    stp     x6,  x7,  [sp, #48]
    stp     x8,  x9,  [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    mrs     x0,  esr_el1
    mrs     x1,  elr_el1
    stp     x30, x0,  [sp, #240]
    str     x1,       [sp, #248]
.endm

.macro restore_context
    ldr     x1,       [sp, #248]
    ldp     x30, x0,  [sp, #240]
    msr     elr_el1,  x1
    ldp     x0,  x1,  [sp, #0]
    ldp     x2,  x3,  [sp, #16]
    ldp     x4,  x5,  [sp, #32]
    ldp     x6,  x7,  [sp, #48]
    ldp     x8,  x9,  [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    add     sp,  sp,  #256
.endm

/* EL1 Synchronous exception handler */
.global el1_sync
el1_sync:
    save_context
    mov     x0, sp          /* pass register frame pointer as arg */
    bl      arm64_sync_handler
    restore_context
    eret

/* EL1 IRQ handler */
.global el1_irq
el1_irq:
    save_context
    mov     x0, sp
    bl      arm64_irq_handler
    restore_context
    eret

/* Panic handler — called for all unhandled exceptions.
 * Declared in hal_impl.c (arm64) as a C function. */
.global arm64_exception_panic
arm64_exception_panic:
    /* Disable all interrupts and spin */
    msr     daifset, #0xf
.Lpanic_spin:
    wfe
    b       .Lpanic_spin
