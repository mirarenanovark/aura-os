/**
 * @file        kernel/include/aura/dashboard.h
 * @layer       STAGE_4_SERVICES_DRIVERS
 * @component   MONITOR_DASHBOARD
 * @contract    TEL-001-telemetry / PRD-11-desktop-compositor
 * @description System Status Dashboard interface.
 *              Coordinates full btop-style monitoring UI rendering on 80x25 VGA.
 *
 * @connects
 *              - Upstream:   kernel/main.c
 *              - Downstream: kernel/core/dashboard.c
 */

#ifndef AURA_DASHBOARD_H
#define AURA_DASHBOARD_H

/* Initialize dashboard (call after sysmon_init) */
void dashboard_init(void);

/* Render the full btop/htop-style dashboard (80x25 VGA) */
void dashboard_render(void);

/* Update dashboard stats (call periodically) */
void dashboard_update(void);

#endif /* AURA_DASHBOARD_H */
