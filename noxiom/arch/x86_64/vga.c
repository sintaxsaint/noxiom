#include "vga.h"
#include "io.h"
#include <stddef.h>

#define VGA_ADDR  0xB8000
#define VGA_CTRL  0x3D4
#define VGA_DATA  0x3D5

static volatile uint16_t *vga_buf = (volatile uint16_t *)VGA_ADDR;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;
static uint8_t cur_color = VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK);

static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}

static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(VGA_CTRL, 14);
    outb(VGA_DATA, (uint8_t)(pos >> 8));
    outb(VGA_CTRL, 15);
    outb(VGA_DATA, (uint8_t)(pos & 0xFF));
}

void vga_init(void) {
    cur_color = VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buf[i] = vga_entry(' ', cur_color);
    cursor_x = cursor_y = 0;
    update_cursor();
}

void vga_set_color(uint8_t color) {
    cur_color = color;
}

static void scroll(void) {
    for (int y = 0; y < VGA_HEIGHT - 1; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = vga_buf[(y + 1) * VGA_WIDTH + x];
    for (int x = 0; x < VGA_WIDTH; x++)
        vga_buf[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', cur_color);
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buf[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', cur_color);
        }
    } else if (c == '\t') {
        cursor_x = (uint8_t)((cursor_x + 8) & ~7);
        if (cursor_x >= VGA_WIDTH) { cursor_x = 0; cursor_y++; }
    } else {
        vga_buf[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, cur_color);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) { cursor_x = 0; cursor_y++; }
    }

    if (cursor_y >= VGA_HEIGHT) {
        scroll();
        cursor_y = VGA_HEIGHT - 1;
    }
    update_cursor();
}

void vga_print(const char *str) {
    while (*str) vga_putchar(*str++);
}

void vga_print_at(const char *str, uint8_t x, uint8_t y, uint8_t color) {
    uint8_t saved_color = cur_color;
    cur_color = color;
    for (int i = 0; str[i]; i++) {
        vga_buf[y * VGA_WIDTH + x + i] = vga_entry(str[i], color);
    }
    cur_color = saved_color;
}

void vga_get_cursor(uint8_t *x, uint8_t *y) {
    *x = cursor_x;
    *y = cursor_y;
}
