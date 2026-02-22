#include "shell.h"
#include "../hal.h"
#include "../string.h"

#define CMD_BUF  256
#define MAX_ARGS 16

static char line[CMD_BUF];
static int  line_len;

static void prompt(void) {
    hal_display_set_color(HAL_COLOR(HAL_COLOR_LIGHT_GREEN, HAL_COLOR_BLACK));
    hal_display_print("noxiom");
    hal_display_set_color(HAL_COLOR(HAL_COLOR_WHITE, HAL_COLOR_BLACK));
    hal_display_print("> ");
    hal_display_set_color(HAL_COLOR(HAL_COLOR_LIGHT_GREY, HAL_COLOR_BLACK));
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
    hal_display_set_color(HAL_COLOR(HAL_COLOR_YELLOW, HAL_COLOR_BLACK));
    hal_display_print("Noxiom OS built-in commands:\n");
    hal_display_set_color(HAL_COLOR(HAL_COLOR_LIGHT_GREY, HAL_COLOR_BLACK));
    hal_display_print("  help      - show this message\n");
    hal_display_print("  clear     - clear the screen\n");
    hal_display_print("  echo ...  - print arguments\n");
    hal_display_print("  version   - show OS version\n");
    hal_display_print("  halt      - halt the system\n");
}

static void cmd_clear(void) {
    hal_display_clear();
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        hal_display_print(argv[i]);
        if (i < argc - 1) hal_display_putchar(' ');
    }
    hal_display_putchar('\n');
}

static void cmd_version(void) {
    hal_display_set_color(HAL_COLOR(HAL_COLOR_CYAN, HAL_COLOR_BLACK));
    hal_display_print("Noxiom OS v0.1.0\n");
    hal_display_set_color(HAL_COLOR(HAL_COLOR_LIGHT_GREY, HAL_COLOR_BLACK));
    hal_display_print("Lightweight server OS - built from scratch\n");
}

static void cmd_halt(void) {
    hal_display_set_color(HAL_COLOR(HAL_COLOR_LIGHT_RED, HAL_COLOR_BLACK));
    hal_display_print("System halted.\n");
    hal_halt();
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
        hal_display_set_color(HAL_COLOR(HAL_COLOR_LIGHT_RED, HAL_COLOR_BLACK));
        hal_display_print("Unknown command: ");
        hal_display_print(argv[0]);
        hal_display_print("\n");
        hal_display_set_color(HAL_COLOR(HAL_COLOR_LIGHT_GREY, HAL_COLOR_BLACK));
    }
}

/* ─── Shell main loop ────────────────────────────────────────────── */

void shell_run(void) {
    line_len = 0;
    prompt();

    for (;;) {
        char c = hal_input_getchar();

        if (c == '\n') {
            hal_display_putchar('\n');
            line[line_len] = '\0';
            dispatch(line);
            line_len = 0;
            prompt();

        } else if (c == '\b') {
            if (line_len > 0) {
                line_len--;
                hal_display_putchar('\b');
            }

        } else if (line_len < CMD_BUF - 1) {
            line[line_len++] = c;
            hal_display_putchar(c);
        }
    }
}
