#include <aura/sysmon.h>
#include <aura/pit.h>
#include <aura/pmm.h>

/* Rolling CPU-load window: 1000 ticks = 1 second at 1000 Hz PIT */
#define SYSMON_WINDOW 1000

static volatile uint64_t idle_ticks;    /* ticks where CPU was idle (hlt) */
static volatile uint8_t  current_idle;  /* set to 1 before hlt, 0 on next tick */
static volatile uint64_t window_idle;   /* idle ticks in current rolling window */
static uint32_t          window_count;  /* ticks in current rolling window */

/* Previous window values (latched once per second) */
static uint32_t prev_window_idle;
static uint32_t prev_window_total;

static uint64_t init_tick;              /* tick count at sysmon_init() */

void sysmon_init(void)
{
    idle_ticks       = 0;
    current_idle     = 0;
    window_idle      = 0;
    window_count     = 0;
    prev_window_idle = 0;
    prev_window_total = SYSMON_WINDOW;
    init_tick        = pit_get_ticks();

    pit_set_callback(sysmon_tick);
}

void sysmon_tick(void)
{
    /* Count the just-finished tick */
    if (current_idle) {
        window_idle++;
        idle_ticks++;
    }
    current_idle = 0;

    window_count++;

    /* Once per window, latch the percentages */
    if (window_count >= SYSMON_WINDOW) {
        prev_window_idle  = (uint32_t)window_idle;
        prev_window_total = window_count;
        window_idle       = 0;
        window_count      = 0;
    }
}

void sysmon_set_idle(void)
{
    current_idle = 1;
}

void sysmon_get_stats(struct sysmon_stats *out)
{
    if (!out) return;

    uint64_t now = pit_get_ticks();
    out->uptime_ms = now - init_tick;

    /* CPU load = 100% - idle% */
    if (prev_window_total > 0) {
        uint32_t idle_pct = (prev_window_idle * 100) / prev_window_total;
        if (idle_pct > 100) idle_pct = 100;
        out->cpu_load_pct = (uint8_t)(100 - idle_pct);
    } else {
        out->cpu_load_pct = 0;
    }

    size_t total = pmm_get_total_memory();
    size_t free  = pmm_get_free_memory();
    out->ram_total_kb = (uint32_t)(total / 1024);
    out->ram_used_kb  = (uint32_t)((total - free) / 1024);
}
