#include "shell.h"
#include "../vga.h"
#include "../keyboard.h"
#include "../string.h"

#define CMD_BUF 256
#define MAX_ARGS 16

static char line[CMD_BUF];
static int  line_len;

static void prompt(void) {
    vga_set_color(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
    vga_print("noxiom");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_print("> ");
    vga_set_color(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
}

/* Split line into argv, return argc */
static int parse(char *buf, char **argv) {
    int argc = 0;
    char *p = buf;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (argc < MAX_ARGS) argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

/* ─── Built-in commands ──────────────────────────────────────────── */

static void cmd_help(void) {
    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_print("Noxiom OS built-in commands:\n");
    vga_set_color(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    vga_print("  help      - show this message\n");
    vga_print("  clear     - clear the screen\n");
    vga_print("  echo ...  - print arguments\n");
    vga_print("  version   - show OS version\n");
    vga_print("  halt      - halt the system\n");
}

static void cmd_clear(void) {
    vga_clear();
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        vga_print(argv[i]);
        if (i < argc - 1) vga_putchar(' ');
    }
    vga_putchar('\n');
}

static void cmd_version(void) {
    vga_set_color(VGA_COLOR(VGA_CYAN, VGA_BLACK));
    vga_print("Noxiom OS v0.1.0\n");
    vga_set_color(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    vga_print("Lightweight server OS - built from scratch\n");
}

static void cmd_halt(void) {
    vga_set_color(VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK));
    vga_print("System halted.\n");
    __asm__ volatile ("cli; hlt");
}

/* ─── Command dispatch ───────────────────────────────────────────── */

static void dispatch(char *buf) {
    if (!buf[0]) return;

    char *argv[MAX_ARGS];
    int   argc = parse(buf, argv);
    if (argc == 0) return;

    if      (kstrcmp(argv[0], "help")    == 0) cmd_help();
    else if (kstrcmp(argv[0], "clear")   == 0) cmd_clear();
    else if (kstrcmp(argv[0], "echo")    == 0) cmd_echo(argc, argv);
    else if (kstrcmp(argv[0], "version") == 0) cmd_version();
    else if (kstrcmp(argv[0], "halt")    == 0) cmd_halt();
    else {
        vga_set_color(VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK));
        vga_print("Unknown command: ");
        vga_print(argv[0]);
        vga_print("\n");
        vga_set_color(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    }
}

/* ─── Shell main loop ────────────────────────────────────────────── */

void shell_run(void) {
    line_len = 0;
    prompt();

    for (;;) {
        char c = keyboard_getchar();

        if (c == '\n') {
            vga_putchar('\n');
            line[line_len] = '\0';
            dispatch(line);
            line_len = 0;
            prompt();

        } else if (c == '\b') {
            if (line_len > 0) {
                line_len--;
                vga_putchar('\b');
            }

        } else if (line_len < CMD_BUF - 1) {
            line[line_len++] = c;
            vga_putchar(c);
        }
    }
}
