#ifndef AURA_DASHBOARD_H
#define AURA_DASHBOARD_H

/* Initialize dashboard (call after sysmon_init) */
void dashboard_init(void);

/* Render the full btop/htop-style dashboard (80x25 VGA) */
void dashboard_render(void);

/* Update dashboard stats (call periodically) */
void dashboard_update(void);

#endif /* AURA_DASHBOARD_H */
