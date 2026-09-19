#include <aura/dashboard.h>
#include <aura/tui.h>
#include <aura/vga.h>
#include <aura/sysmon.h>
#include <aura/pit.h>
#include <aura/pmm.h>
#include <aura/heap.h>

/* Fake "process" table for the dashboard */
struct dashboard_process {
    int pid;
    const char *name;
    const char *state;
    uint8_t cpu_pct;
    uint32_t mem_kb;
    int priority;
};

static struct dashboard_process processes[] = {
    { 0, "kernel_idle",      "RUNNING", 0, 0, 0 },
    { 1, "sysmon_sampler",   "RUNNING", 0, 0, 10 },
    { 2, "pit_timer",        "RUNNING", 0, 0, 5 },
    { 3, "vga_compositor",   "RUNNING", 0, 0, 8 },
};

#define PROCESS_COUNT (sizeof(processes) / sizeof(processes[0]))

static uint64_t last_uptime_s = 0;

void dashboard_init(void) {
    last_uptime_s = 0;
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
    uint8_t border_color = 0x0B; /* Light cyan on black */
    uint8_t text_color = 0x07;   /* Light grey on black */
    uint8_t bar_color = 0x0A;    /* Light green on black */
    uint8_t bar_empty = 0x08;    /* Dark grey on black */
    
    tui_draw_box(0, 1, 40, 8, border_color, "CPU", 0);
    
    struct sysmon_stats stats;
    sysmon_get_stats(&stats);
    
    tui_printf_at(2, 2, text_color, "PIT: %u Hz", PIT_TICKS_PER_SECOND);
    tui_printf_at(2, 3, text_color, "Load: %u%%", (uint32_t)stats.cpu_load_pct);
    tui_printf_at(20, 3, text_color, "IRQs: %u", (uint32_t)(pit_get_ticks() / 1000));
    
    tui_puts_at(2, 4, "[", text_color);
    tui_draw_bar(3, 4, 34, stats.cpu_load_pct, 100, bar_color, bar_empty);
    tui_puts_at(37, 4, "]", text_color);
}

static void draw_memory_box(void) {
    uint8_t border_color = 0x0D; /* Light magenta on black */
    uint8_t text_color = 0x07;
    uint8_t bar_color = 0x09;    /* Light blue on black */
    uint8_t bar_empty = 0x08;
    
    tui_draw_box(40, 1, 40, 8, border_color, "Memory", 0);
    
    struct sysmon_stats stats;
    sysmon_get_stats(&stats);
    
    uint32_t total_mb = stats.ram_total_kb / 1024;
    uint32_t used_mb = stats.ram_used_kb / 1024;
    uint32_t free_mb = total_mb - used_mb;
    
    tui_printf_at(42, 2, text_color, "Total: %u MB", total_mb);
    tui_printf_at(42, 3, text_color, "Used:  %u MB", used_mb);
    tui_printf_at(42, 4, text_color, "Free:  %u MB", free_mb);
    
    size_t heap_used = heap_get_used();
    size_t heap_free = heap_get_free();
    
    tui_printf_at(42, 5, text_color, "Heap:  %u KB", (uint32_t)(heap_used / 1024));
    tui_printf_at(42, 6, text_color, "Free:  %u KB", (uint32_t)(heap_free / 1024));
    
    tui_puts_at(42, 7, "[", text_color);
    tui_draw_bar(43, 7, 34, stats.ram_used_kb, stats.ram_total_kb, bar_color, bar_empty);
    tui_puts_at(77, 7, "]", text_color);
}

static void draw_process_table(void) {
    uint8_t border_color = 0x0E; /* Yellow on black */
    uint8_t header_color = 0x1F; /* White on blue */
    uint8_t text_color = 0x07;
    uint8_t highlight_color = 0x0F; /* White on black */
    
    tui_draw_box(0, 9, 80, 15, border_color, "Kernel Threads", 0);
    
    /* Header row */
    tui_printf_at(2, 10, header_color, " %-4s %-18s %-8s %-6s %-8s %-8s",
                  "PID", "NAME", "STATE", "CPU%", "MEM", "PRIORITY");
    
    tui_puts_at(1, 11, "────────────────────────────────────────────────────────────────────────────────",
                border_color);
    
    /* Update fake CPU/memory stats */
    struct sysmon_stats stats;
    sysmon_get_stats(&stats);
    
    processes[0].cpu_pct = 100 - stats.cpu_load_pct; /* idle */
    processes[1].cpu_pct = 2;
    processes[2].cpu_pct = 1;
    processes[3].cpu_pct = stats.cpu_load_pct > 5 ? stats.cpu_load_pct / 3 : 1;
    
    processes[0].mem_kb = 0;
    processes[1].mem_kb = 4;
    processes[2].mem_kb = 2;
    processes[3].mem_kb = stats.ram_used_kb / 10;
    
    /* Process rows */
    for (int i = 0; i < (int)PROCESS_COUNT && i < 12; i++) {
        uint8_t row_color = (i % 2 == 0) ? text_color : highlight_color;
        
        tui_printf_at(2, 12 + i, row_color, " %-4d %-18s %-8s %-6u %-8u %-8d",
                      processes[i].pid,
                      processes[i].name,
                      processes[i].state,
                      (uint32_t)processes[i].cpu_pct,
                      processes[i].mem_kb,
                      processes[i].priority);
    }
}

void dashboard_update(void) {
    /* Stats are fetched live during render */
}

void dashboard_render(void) {
    /* Clear screen */
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
    
    /* Header */
    struct sysmon_stats stats;
    sysmon_get_stats(&stats);
    
    char uptime_str[9];
    format_uptime(uptime_str, stats.uptime_ms);
    
    tui_draw_header("AuraOS v0.1.0-alpha [btop mode]", uptime_str);
    
    /* CPU box */
    draw_cpu_box();
    
    /* Memory box */
    draw_memory_box();
    
    /* Process table */
    draw_process_table();
    
    /* Footer */
    tui_draw_footer("[Tab] Switch View  [R] Redraw  [Q] Halt System");
}
