#include "gdt.h"
#include <stdint.h>

/* GDT entry (8 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} gdt_entry_t;

/* GDT pointer loaded into GDTR */
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} gdt_ptr_t;

/* Kernel GDT: null, code, data */
static gdt_entry_t gdt[3];
static gdt_ptr_t   gdt_ptr;

extern void gdt_flush(uint64_t ptr);

static void gdt_set(int i, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t gran) {
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

void gdt_init(void) {
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    gdt_set(0, 0, 0,      0x00, 0x00);  /* null */
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xA0); /* 64-bit code: execute/read, present */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xA0); /* 64-bit data: read/write,  present */

    gdt_flush((uint64_t)&gdt_ptr);
}
