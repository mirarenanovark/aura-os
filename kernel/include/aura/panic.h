/**
 * @file        kernel/include/aura/panic.h
 * @layer       STAGE_2_PHYSICAL_FAULT
 * @component   CORE_PANIC
 * @contract    PRD-06-verification
 * @description Kernel panic assertion and fatal halt interface.
 *
 * @connects
 *              - Upstream:   All kernel subsystems via AURA_PANIC macro
 *              - Downstream: kernel/core/panic.c
 *              - Hardware:   CPU halt ('cli; hlt')
 */

#ifndef AURA_PANIC_H
#define AURA_PANIC_H

#include <stdint.h>
#include <aura/idt.h>

/* Save the execution point for kernel recovery if panic occurs.
 * If panic happens and the user selects [R] Restore, execution resumes here.
 * Returns 0 on initial checkpoint creation, or 1 when returning from a restored panic. */
int panic_set_recovery_point(void);

/* Interactive kernel panic with registers dump and user actions:
 * [R] Restore, [B] Reboot, [O] Power Off */
void aura_panic_interactive(const char *msg, const char *file, uint32_t line, registers_t *regs);

/* Kernel panic: print message with file/line and prompt for interactive recovery. */
void aura_panic(const char *msg, const char *file, uint32_t line);

/* Convenience macro captures __FILE__ and __LINE__ automatically. */
#define AURA_PANIC(msg) aura_panic((msg), __FILE__, __LINE__)

#endif /* AURA_PANIC_H */

