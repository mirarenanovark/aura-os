/**
 * @file        kernel/core/dashboard.c
 * @layer       STAGE_4_SERVICES_DRIVERS
 * @component   MONITOR_DASHBOARD
 * @contract    TEL-001-telemetry / PRD-11-desktop-compositor
 * @description Renders a live btop/htop-inspired system monitor on the 80x25 VGA display.
 *              Divides screen into a top header, split CPU metrics box, RAM & Heap
 *              memory gauges, process execution table, and bottom hotkey bar.
 *
 * @connects
 *              - Upstream:   kernel/main.c:kernel_main (dashboard_init, dashboard_render)
 *              - Downstream: kernel/drivers/tui.c, kernel/drivers/vga.c, kernel/core/sysmon.c,
 *                            kernel/core/pmm.c, kernel/core/heap.c
 *              - Hardware:   Draws directly into VGA MMIO via TUI library
 *
 * @flow        [AURA_FLOW: DASHBOARD_DRAW]
 *              1. tui_draw_header(): Renders blue inverted top status bar with uptime.
 *              2. draw_cpu_box(): Draws single-border box, queries sysmon for CPU load %, renders gauge.
 *              3. draw_mem_box(): Queries PMM for physical RAM usage and Heap for dynamic memory, renders bars.
 *              4. draw_proc_box(): Renders PID table with process states, CPU %, and memory footprint.
 *              5. tui_draw_footer(): Renders bottom hotkey reminder bar.
 */

#include <aura/dashboard.h>
#include <aura/tui.h>
#include <aura/vga.h>
#include <aura/sysmon.h>
#include <aura/pit.h>
#include <aura/pmm.h>
#include <aura/heap.h>

/*
 * AuraOS btop-style dashboard.
 * Palette rule: high-contrast neon-on-black.
 * Full-bright foregrounds only (0x0A green, 0x0B cyan, 0x0E yellow, 0x0F white),
 * no mid-grey text (that's what made v0.2.0 look washed out).
 */

/* Kernel task table (static until the scheduler exists) */
struct dashboard_process {
    int pid;
    const char *name;
    const char *state;
    uint8_t cpu_pct;
    uint32_t mem_kb;
    int priority;
};

static struct dashboard_process processes[] = {
    { 0, "kernel_idle",    "RUN", 0, 0,   0 },
    { 1, "sysmon_sampler", "RUN", 0, 4,  10 },
    { 2, "pit_timer",      "RUN", 0, 2,   5 },
    { 3, "vga_dashboard",  "RUN", 0, 64,  8 },
};

#define PROCESS_COUNT (sizeof(processes) / sizeof(processes[0]))

void dashboard_init(void) {
}

static void format_uptime(char *buf, uint64_t ms) {
    uint64_t total_s = ms / 1000;
    uint64_t h = total_s / 3600;
    uint64_t m = (total_s % 3600) / 60;
    uint64_t s = total_s % 60;

    buf[0] = '0' + (char)(h / 10);
    buf[1] = '0' + (char)(h % 10);
    buf[2] = ':';
    buf[3] = '0' + (char)(m / 10);
    buf[4] = '0' + (char)(m % 10);
    buf[5] = ':';
    buf[6] = '0' + (char)(s / 10);
    buf[7] = '0' + (char)(s % 10);
    buf[8] = '\0';
}

static void draw_cpu_box(void) {
    struct sysmon_stats stats;
    sysmon_get_stats(&stats);

    /* Cyan border, bright white title */
    tui_draw_box(0, 1, 40, 8, 0x0B, 0x0F, " cpu ", 0);

    tui_printf_at(2, 3, 0x0F, "Load");
    tui_printf_at(8, 3, 0x0B, "%u%%", (uint32_t)stats.cpu_load_pct);
    tui_printf_at(20, 3, 0x0F, "Ticks");
    tui_printf_at(27, 3, 0x0B, "%u", (uint32_t)pit_get_ticks());

    /* Bar: bright green fill over a dim track */
    tui_draw_bar(2, 5, 36, stats.cpu_load_pct, 100, 0x0A, 0x08);

    /* Percentage at the end of the bar */
    tui_printf_at(17, 6, 0x0A, "%u%%", (uint32_t)stats.cpu_load_pct);
}

static void draw_memory_box(void) {
    struct sysmon_stats stats;
    sysmon_get_stats(&stats);

    /* Magenta border, bright white title */
    tui_draw_box(40, 1, 40, 8, 0x0D, 0x0F, " mem ", 0);

    uint32_t total_mb = stats.ram_total_kb / 1024;
    uint32_t used_mb = stats.ram_used_kb / 1024;

    tui_printf_at(42, 3, 0x0F, "RAM");
    tui_printf_at(47, 3, 0x0D, "%u/%u MB", used_mb, total_mb);
    tui_printf_at(42, 4, 0x0F, "Heap");
    tui_printf_at(48, 4, 0x0D, "%u KB used", (uint32_t)(heap_get_used() / 1024));
    tui_printf_at(42, 5, 0x0F, "zRAM");
    tui_printf_at(48, 5, 0x0A, "%uKB saved (%u.%ux)",
                  stats.zram_saved_kb,
                  stats.zram_ratio_x100 / 100,
                  (stats.zram_ratio_x100 % 100) / 10);

    /* Bar: bright magenta fill over a dim track */
    tui_draw_bar(42, 7, 36, stats.ram_used_kb, stats.ram_total_kb, 0x0D, 0x08);
}

static void draw_process_table(void) {
    struct sysmon_stats stats;
    sysmon_get_stats(&stats);

    /* Yellow border, bright white title */
    tui_draw_box(0, 9, 80, 14, 0x0E, 0x0F, " tasks ", 0);

    /* Header row: black text on bright cyan strip */
    tui_printf_at(2, 11, 0xB0, "PID  NAME             STATE  CPU%   MEM(KB) PRI");
    for (int x = 50; x < 78; x++) tui_putc_at(x, 11, ' ', 0xB0);

    /* Live values for the static kernel tasks */
    processes[0].cpu_pct = (uint8_t)(100 - stats.cpu_load_pct);
    processes[3].cpu_pct = (uint8_t)(stats.cpu_load_pct > 5 ? stats.cpu_load_pct / 3 : 1);

    for (int i = 0; i < (int)PROCESS_COUNT && i < 10; i++) {
        /* Alternating rows: white / bright cyan — always readable on black */
        uint8_t row_color = (i % 2 == 0) ? 0x0F : 0x0B;
        tui_printf_at(2, 12 + i, row_color, "%-4d %-16s %-6s %-6u %-7u %-3d",
                      processes[i].pid,
                      processes[i].name,
                      processes[i].state,
                      (uint32_t)processes[i].cpu_pct,
                      processes[i].mem_kb,
                      processes[i].priority);
    }
}

void dashboard_update(void) {
}

void dashboard_render(void) {
    struct sysmon_stats stats;
    sysmon_get_stats(&stats);

    char uptime_str[9];
    format_uptime(uptime_str, stats.uptime_ms);

    /* Full-width header: bright yellow title on blue band */
    tui_draw_header(" AuraOS v0.2.3 ", uptime_str);

    draw_cpu_box();
    draw_memory_box();
    draw_process_table();

    tui_draw_footer(" [Q] menu   [R] redraw");
}
