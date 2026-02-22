#include "keyboard_x86.h"
#include "pic.h"
#include "io.h"
#include <stdint.h>

#define KB_DATA 0x60

/* PS/2 scancode set 1 -> ASCII (unshifted) */
static const char sc_table[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n', 0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0
};

/* Shifted version (when shift is held) */
static const char sc_table_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0
};

/* Scancodes for shift keys */
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LSHIFT_REL 0xAA
#define SC_RSHIFT_REL 0xB6

static int shift_held = 0;

/* Ring buffer */
#define KB_BUF_SIZE 256
static volatile char kb_buf[KB_BUF_SIZE];
static volatile int  kb_head = 0;
static volatile int  kb_tail = 0;

static void buf_push(char c) {
    int next = (kb_head + 1) % KB_BUF_SIZE;
    if (next != kb_tail) {
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

void keyboard_init(void) {
    pic_unmask(1);  /* unmask IRQ1 */
}

void keyboard_irq_handler(void) {
    uint8_t sc = inb(KB_DATA);

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift_held = 1;
        return;
    }
    if (sc == SC_LSHIFT_REL || sc == SC_RSHIFT_REL) {
        shift_held = 0;
        return;
    }

    /* Ignore key-release events (bit 7 set) */
    if (sc & 0x80)
        return;

    char c = shift_held ? sc_table_shift[sc] : sc_table[sc];
    if (c)
        buf_push(c);
}

char keyboard_getchar(void) {
    while (kb_head == kb_tail)
        __asm__ volatile ("hlt");   /* wait for IRQ to fire */

    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return c;
}
