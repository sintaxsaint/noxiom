#include "vga.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "shell/shell.h"

static void print_banner(void) {
    /* Top border */
    vga_set_color(VGA_COLOR(VGA_CYAN, VGA_BLACK));
    vga_print("================================================================================");

    /* Title */
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_print("\n");
    vga_print("                              N O X I O M   O S\n");
    vga_print("                         Lightweight Server Operating System\n");
    vga_print("                                  Version 0.1.0\n");
    vga_print("\n");

    /* Bottom border */
    vga_set_color(VGA_COLOR(VGA_CYAN, VGA_BLACK));
    vga_print("================================================================================");

    vga_set_color(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    vga_print("\n\n");
    vga_print("Type 'help' for a list of commands.\n\n");
}

void kmain(void) {
    serial_init();
    serial_print("[noxiom] kernel started\n");

    vga_init();
    serial_print("[noxiom] vga ok\n");

    gdt_init();
    serial_print("[noxiom] gdt ok\n");

    pic_init();
    serial_print("[noxiom] pic ok\n");

    idt_init();
    serial_print("[noxiom] idt ok\n");

    keyboard_init();
    serial_print("[noxiom] keyboard ok\n");

    print_banner();
    serial_print("[noxiom] entering shell\n");

    shell_run();

    /* shell_run() never returns, but just in case */
    for (;;) __asm__ volatile ("hlt");
}
