#include "idt.h"
#include "pic.h"
#include "vga.h"
#include "keyboard.h"
#include <stdint.h>

/* IDT gate descriptor (16 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

extern void idt_load(uint64_t ptr);

/* Declare all ISR/IRQ stubs from entry.asm */
#define DECL_ISR(n) extern void isr##n(void);
#define DECL_IRQ(n) extern void irq##n(void);

DECL_ISR(0)  DECL_ISR(1)  DECL_ISR(2)  DECL_ISR(3)  DECL_ISR(4)
DECL_ISR(5)  DECL_ISR(6)  DECL_ISR(7)  DECL_ISR(8)  DECL_ISR(9)
DECL_ISR(10) DECL_ISR(11) DECL_ISR(12) DECL_ISR(13) DECL_ISR(14)
DECL_ISR(15) DECL_ISR(16) DECL_ISR(17) DECL_ISR(18) DECL_ISR(19)
DECL_ISR(20) DECL_ISR(21) DECL_ISR(22) DECL_ISR(23) DECL_ISR(24)
DECL_ISR(25) DECL_ISR(26) DECL_ISR(27) DECL_ISR(28) DECL_ISR(29)
DECL_ISR(30) DECL_ISR(31)

DECL_IRQ(0)  DECL_IRQ(1)  DECL_IRQ(2)  DECL_IRQ(3)  DECL_IRQ(4)
DECL_IRQ(5)  DECL_IRQ(6)  DECL_IRQ(7)  DECL_IRQ(8)  DECL_IRQ(9)
DECL_IRQ(10) DECL_IRQ(11) DECL_IRQ(12) DECL_IRQ(13) DECL_IRQ(14)
DECL_IRQ(15)

static void idt_set_gate(int n, uint64_t handler, uint8_t flags) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].selector    = 0x08;  /* kernel code segment */
    idt[n].ist         = 0;
    idt[n].flags       = flags;
    idt[n].reserved    = 0;
}

void idt_init(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    /* CPU exceptions */
    idt_set_gate(0,  (uint64_t)isr0,  0x8E);
    idt_set_gate(1,  (uint64_t)isr1,  0x8E);
    idt_set_gate(2,  (uint64_t)isr2,  0x8E);
    idt_set_gate(3,  (uint64_t)isr3,  0x8E);
    idt_set_gate(4,  (uint64_t)isr4,  0x8E);
    idt_set_gate(5,  (uint64_t)isr5,  0x8E);
    idt_set_gate(6,  (uint64_t)isr6,  0x8E);
    idt_set_gate(7,  (uint64_t)isr7,  0x8E);
    idt_set_gate(8,  (uint64_t)isr8,  0x8E);
    idt_set_gate(9,  (uint64_t)isr9,  0x8E);
    idt_set_gate(10, (uint64_t)isr10, 0x8E);
    idt_set_gate(11, (uint64_t)isr11, 0x8E);
    idt_set_gate(12, (uint64_t)isr12, 0x8E);
    idt_set_gate(13, (uint64_t)isr13, 0x8E);
    idt_set_gate(14, (uint64_t)isr14, 0x8E);
    idt_set_gate(15, (uint64_t)isr15, 0x8E);
    idt_set_gate(16, (uint64_t)isr16, 0x8E);
    idt_set_gate(17, (uint64_t)isr17, 0x8E);
    idt_set_gate(18, (uint64_t)isr18, 0x8E);
    idt_set_gate(19, (uint64_t)isr19, 0x8E);
    idt_set_gate(20, (uint64_t)isr20, 0x8E);
    idt_set_gate(21, (uint64_t)isr21, 0x8E);
    idt_set_gate(22, (uint64_t)isr22, 0x8E);
    idt_set_gate(23, (uint64_t)isr23, 0x8E);
    idt_set_gate(24, (uint64_t)isr24, 0x8E);
    idt_set_gate(25, (uint64_t)isr25, 0x8E);
    idt_set_gate(26, (uint64_t)isr26, 0x8E);
    idt_set_gate(27, (uint64_t)isr27, 0x8E);
    idt_set_gate(28, (uint64_t)isr28, 0x8E);
    idt_set_gate(29, (uint64_t)isr29, 0x8E);
    idt_set_gate(30, (uint64_t)isr30, 0x8E);
    idt_set_gate(31, (uint64_t)isr31, 0x8E);

    /* Hardware IRQs (remapped to 32-47 by PIC) */
    idt_set_gate(32, (uint64_t)irq0,  0x8E);
    idt_set_gate(33, (uint64_t)irq1,  0x8E);
    idt_set_gate(34, (uint64_t)irq2,  0x8E);
    idt_set_gate(35, (uint64_t)irq3,  0x8E);
    idt_set_gate(36, (uint64_t)irq4,  0x8E);
    idt_set_gate(37, (uint64_t)irq5,  0x8E);
    idt_set_gate(38, (uint64_t)irq6,  0x8E);
    idt_set_gate(39, (uint64_t)irq7,  0x8E);
    idt_set_gate(40, (uint64_t)irq8,  0x8E);
    idt_set_gate(41, (uint64_t)irq9,  0x8E);
    idt_set_gate(42, (uint64_t)irq10, 0x8E);
    idt_set_gate(43, (uint64_t)irq11, 0x8E);
    idt_set_gate(44, (uint64_t)irq12, 0x8E);
    idt_set_gate(45, (uint64_t)irq13, 0x8E);
    idt_set_gate(46, (uint64_t)irq14, 0x8E);
    idt_set_gate(47, (uint64_t)irq15, 0x8E);

    idt_load((uint64_t)&idt_ptr);
}

static const char *exception_names[] = {
    "Divide-by-Zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 FP Exception", "Alignment Check", "Machine Check", "SIMD FP Exception",
    "Virtualization", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Security Exception", "Reserved"
};

void isr_handler(registers_t *regs) {
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_RED));
    vga_print("\n*** KERNEL EXCEPTION: ");
    if (regs->int_no < 32)
        vga_print(exception_names[regs->int_no]);
    vga_print(" ***\n");
    /* halt */
    __asm__ volatile ("cli; hlt");
}

void irq_handler(registers_t *regs) {
    if (regs->int_no == 33)
        keyboard_irq_handler();

    pic_send_eoi((uint8_t)(regs->int_no - 32));
}
