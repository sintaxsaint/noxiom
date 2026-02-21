#pragma once
#include <stdint.h>

/* Register state saved by ISR/IRQ stubs in entry.asm */
typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    /* CPU-pushed frame */
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

void idt_init(void);

/* Called from entry.asm */
void isr_handler(registers_t *regs);
void irq_handler(registers_t *regs);
