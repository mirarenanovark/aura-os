#ifndef AURA_SYSMON_H
#define AURA_SYSMON_H

#include <stdint.h>

struct sysmon_stats {
    uint64_t uptime_ms;       /* milliseconds since sysmon_init() */
    uint8_t  cpu_load_pct;    /* 0-100, rolling average over SYSMON_WINDOW ticks */
    uint32_t ram_used_kb;     /* PMM: total - free */
    uint32_t ram_total_kb;    /* PMM: total physical memory */
};

/* Initialize sysmon; call after PIT + PMM are ready. Registers as PIT callback. */
void sysmon_init(void);

/* Called every PIT tick from the IRQ handler (registered via pit_set_callback). */
void sysmon_tick(void);

/* Mark the CPU as idle before entering hlt.  Clears automatically on next tick. */
void sysmon_set_idle(void);

/* Snapshot current stats into |out|. */
void sysmon_get_stats(struct sysmon_stats *out);

#endif /* AURA_SYSMON_H */
