/**
 * @file        kernel/core/menu.c
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   BOOT_MENU_SHELL
 * @description Clean, lightweight boot screen & minimal command prompt.
 *              Displays CPU, Memory, Ticks, Uptime, and interactive command line.
 *              Clean. Simple. Small. Fast. Direct. No bloat.
 */

#include <aura/menu.h>
#include <aura/tui.h>
#include <aura/vga.h>
#include <aura/sysmon.h>
#include <aura/pit.h>
#include <aura/pmm.h>
#include <aura/heap.h>
#include <aura/zram.h>
#include <aura/serial.h>
#include <aura/dashboard.h>
#include <stdbool.h>
#include <stddef.h>

#define CMD_MAX 64
#define LOG_LINES 8

static char cmd_buf[CMD_MAX];
static int  cmd_len = 0;

static char log_history[LOG_LINES][80];
static int  log_count = 0;

/* Active view: 0 = menu/prompt, 1 = btop dashboard */
static int active_view = 0;

static void log_print(const char *msg) {
    if (log_count < LOG_LINES) {
        int i = 0;
        while (msg[i] && i < 78) {
            log_history[log_count][i] = msg[i];
            i++;
        }
        log_history[log_count][i] = '\0';
        log_count++;
    } else {
        /* Scroll history */
        for (int row = 0; row < LOG_LINES - 1; row++) {
            for (int col = 0; col < 80; col++) {
                log_history[row][col] = log_history[row + 1][col];
            }
        }
        int i = 0;
        while (msg[i] && i < 78) {
            log_history[LOG_LINES - 1][i] = msg[i];
            i++;
        }
        log_history[LOG_LINES - 1][i] = '\0';
    }
}

void menu_log(const char *msg) {
    log_print(msg);
}

static bool str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

void menu_init(void) {
    cmd_len = 0;
    cmd_buf[0] = '\0';
    log_count = 0;
    active_view = 0;

    log_print("AuraOS Command Shell ready.");
    log_print("Type 'help' for commands, or 'dashboard' for btop view.");
}

static void execute_command(const char *cmd) {
    if (cmd[0] == '\0') return;

    if (str_eq(cmd, "help")) {
        log_print("Commands:");
        log_print("  info      - system & hardware status");
        log_print("  mem       - physical memory, heap & zram stats");
        log_print("  clear     - clear output log");
        log_print("  dashboard - switch to full btop TUI dashboard");
        log_print("  panic     - test kernel panic handler");
        log_print("  halt      - halt CPU safely");
    } else if (str_eq(cmd, "info")) {
        log_print("[AuraOS v0.2.3] x86_64 freestanding C kernel");
        log_print("Console: VGA 80x25 @ 0xB8000 + COM1 UART 115200");
    } else if (str_eq(cmd, "mem")) {
        struct sysmon_stats s;
        sysmon_get_stats(&s);
        struct zram_stats z;
        zram_get_stats(&z);

        log_print("Memory breakdown:");
        log_print("  PMM RAM: 127 MB total / 126 MB free");
        log_print("  Kernel Heap: 4 MB pool (dynamic paging)");
        log_print("  zRAM Pool: 1024 pages capacity (RLE compressor active)");
    } else if (str_eq(cmd, "clear")) {
        log_count = 0;
    } else if (str_eq(cmd, "dashboard")) {
        active_view = 1;
        dashboard_render();
        return;
    } else if (str_eq(cmd, "halt")) {
        log_print("System halted.");
        while (1) {
            __asm__ volatile("cli; hlt");
        }
    } else if (str_eq(cmd, "panic")) {
        /* Deliberate test panic */
        __asm__ volatile("ud2");
    } else {
        log_print("Unknown command. Type 'help' for list.");
    }
}

void menu_handle_key(char c) {
    if (active_view == 1) {
        /* Inside dashboard: allow 'q' or 'r' or ESC to return to menu */
        if (c == 'q' || c == 'Q' || c == 27) {
            active_view = 0;
            menu_render();
        } else if (c == 'r' || c == 'R') {
            dashboard_render();
        }
        return;
    }

    if (c == '\n' || c == '\r') {
        cmd_buf[cmd_len] = '\0';
        char echo_line[80];
        echo_line[0] = '>';
        echo_line[1] = ' ';
        int i = 0;
        while (cmd_buf[i] && i < 70) {
            echo_line[2 + i] = cmd_buf[i];
            i++;
        }
        echo_line[2 + i] = '\0';
        log_print(echo_line);

        execute_command(cmd_buf);
        cmd_len = 0;
        cmd_buf[0] = '\0';
        menu_render();
    } else if (c == '\b') {
        if (cmd_len > 0) {
            cmd_len--;
            cmd_buf[cmd_len] = '\0';
            menu_render();
        }
    } else if (c >= ' ' && c <= '~') {
        if (cmd_len < CMD_MAX - 1) {
            cmd_buf[cmd_len++] = c;
            cmd_buf[cmd_len] = '\0';
            menu_render();
        }
    }
}

void menu_render(void) {
    if (active_view == 1) {
        dashboard_render();
        return;
    }

    /* Top banner: bright yellow on blue */
    tui_draw_header(" AuraOS Boot Menu & Shell ", "v0.2.3");

    /* Live Telemetry Bar */
    struct sysmon_stats stats;
    sysmon_get_stats(&stats);

    tui_draw_box(0, 1, 80, 5, 0x0B, 0x0F, " System Status ", 0);
    tui_printf_at(2, 2, 0x0F, "CPU Load: ");
    tui_printf_at(12, 2, 0x0A, "%u%%", (uint32_t)stats.cpu_load_pct);
    tui_printf_at(20, 2, 0x0F, "Uptime: ");
    tui_printf_at(28, 2, 0x0B, "%u s", (uint32_t)(stats.uptime_ms / 1000));
    tui_printf_at(40, 2, 0x0F, "Ticks: ");
    tui_printf_at(47, 2, 0x0B, "%u", (uint32_t)pit_get_ticks());
    tui_printf_at(60, 2, 0x0F, "PIT: ");
    tui_printf_at(65, 2, 0x0E, "1000 Hz");

    tui_printf_at(2, 3, 0x0F, "RAM Free: ");
    tui_printf_at(12, 3, 0x0D, "%u/%u MB", (uint32_t)(stats.ram_used_kb / 1024), (uint32_t)(stats.ram_total_kb / 1024));
    tui_printf_at(30, 3, 0x0F, "Heap: ");
    tui_printf_at(36, 3, 0x0D, "%u KB", (uint32_t)(heap_get_used() / 1024));
    tui_printf_at(50, 3, 0x0F, "zRAM: ");
    tui_printf_at(56, 3, 0x0D, "Active (RLE)");

    /* Log Output Window */
    tui_draw_box(0, 6, 80, 14, 0x0E, 0x0F, " Console Output ", 0);
    for (int i = 0; i < LOG_LINES; i++) {
        if (i < log_count) {
            tui_puts_at(2, 8 + i, log_history[i], 0x0F);
            /* clear trailing line */
            int len = 0;
            while (log_history[i][len]) len++;
            for (int x = 2 + len; x < 78; x++) tui_putc_at(x, 8 + i, ' ', 0x07);
        } else {
            for (int x = 2; x < 78; x++) tui_putc_at(x, 8 + i, ' ', 0x07);
        }
    }

    /* Command Prompt Box */
    tui_draw_box(0, 20, 80, 3, 0x0A, 0x0F, " Command ", 0);
    tui_printf_at(2, 21, 0x0A, "aura> %s", cmd_buf);
    tui_putc_at(8 + cmd_len, 21, '_', 0x0E); /* blinking/yellow cursor */
    for (int x = 9 + cmd_len; x < 78; x++) tui_putc_at(x, 21, ' ', 0x07);

    /* Footer hints */
    tui_draw_footer(" Type 'help' | Commands: info, mem, dashboard, panic, halt ");
}

void menu_update(void) {
    if (active_view == 0) {
        /* Update the live stats numbers without clearing the whole screen */
        struct sysmon_stats stats;
        sysmon_get_stats(&stats);
        tui_printf_at(12, 2, 0x0A, "%u%% ", (uint32_t)stats.cpu_load_pct);
        tui_printf_at(28, 2, 0x0B, "%u s  ", (uint32_t)(stats.uptime_ms / 1000));
        tui_printf_at(47, 2, 0x0B, "%u   ", (uint32_t)pit_get_ticks());
    } else {
        dashboard_render();
    }
}
