#include "hal.h"
#include "shell/shell.h"

static void print_hw_info(void) {
    hal_display_set_color(HAL_COLOR(HAL_COLOR_YELLOW, HAL_COLOR_BLACK));
    hal_display_print("[hal] CPU: ");
    hal_display_set_color(HAL_COLOR(HAL_COLOR_LIGHT_GREY, HAL_COLOR_BLACK));
    hal_display_print(g_hw_info.model_str);
    hal_display_print("  Tier: ");
    switch (g_hw_info.tier) {
        case TIER_HIGH:     hal_display_print("HIGH\n");     break;
        case TIER_MID:      hal_display_print("MID\n");      break;
        case TIER_LOW:      hal_display_print("LOW\n");      break;
        case TIER_FALLBACK: hal_display_print("FALLBACK\n"); break;
        default:            hal_display_print("UNKNOWN\n");  break;
    }
}

static void print_banner(void) {
    hal_display_set_color(HAL_COLOR(HAL_COLOR_CYAN, HAL_COLOR_BLACK));
    hal_display_print("================================================================================");
    hal_display_set_color(HAL_COLOR(HAL_COLOR_WHITE, HAL_COLOR_BLACK));
    hal_display_print("\n");
    hal_display_print("                              N O X I O M   O S\n");
    hal_display_print("                         Lightweight Server Operating System\n");
    hal_display_print("                                  Version 0.1.0\n");
    hal_display_print("\n");
    hal_display_set_color(HAL_COLOR(HAL_COLOR_CYAN, HAL_COLOR_BLACK));
    hal_display_print("================================================================================");
    hal_display_set_color(HAL_COLOR(HAL_COLOR_LIGHT_GREY, HAL_COLOR_BLACK));
    hal_display_print("\n\nType 'help' for a list of commands.\n\n");
}

void kmain(void) {
    /* 1. Serial first — always works, gives us early debug output */
    hal_serial_init();
    hal_serial_print("[noxiom] kernel started\n");

    /* 2. Detect hardware properties and compute tier */
    hal_hw_detect();
    g_hw_info.tier = hal_hw_score();
    hal_serial_print("[noxiom] hw detected\n");

    /* 3. CPU descriptor tables (GDT+IDT on x86; VBAR_EL1 on arm64) */
    hal_cpu_init();
    hal_serial_print("[noxiom] cpu ok\n");

    /* 4. Interrupt controller (PIC on x86; GIC on arm64) */
    hal_intc_init();
    hal_serial_print("[noxiom] intc ok\n");

    /* 5. Display */
    hal_display_init();
    hal_serial_print("[noxiom] display ok\n");

    /* 6. Input */
    hal_input_init();
    hal_serial_print("[noxiom] input ok\n");

    print_hw_info();
    print_banner();
    hal_serial_print("[noxiom] entering shell\n");

    shell_run();
    hal_halt();
}
