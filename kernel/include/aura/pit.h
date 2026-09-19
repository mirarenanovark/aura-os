/**
 * @file        kernel/include/aura/pit.h
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   ARCH_PIT_8254
 * @contract    PRD-07-boot-platform
 * @description Programmable Interval Timer (8254) driver interface.
 *              Fixed 1000 Hz heartbeat (1ms tick), tick accumulator, callback hook.
 *
 * @connects
 *              - Upstream:   kernel/main.c, kernel/core/sysmon.c
 *              - Downstream: kernel/arch/x86_shared/pit.c
 *              - Hardware:   8254 PIT Ports 0x40 (Channel 0) & 0x43 (Command)
 */

#ifndef AURA_PIT_H
#define AURA_PIT_H

#include <stdint.h>

#define PIT_TICKS_PER_SECOND 1000
#define PIT_BASE_FREQUENCY   1193182

void pit_init(void);
void pit_set_callback(void (*cb)(void));
uint64_t pit_get_ticks(void);

#endif /* AURA_PIT_H */
