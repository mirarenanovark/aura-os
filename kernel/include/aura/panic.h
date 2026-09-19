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

/* Kernel panic: print message with file/line to VGA+serial, then halt. */
void aura_panic(const char *msg, const char *file, uint32_t line);

/* Convenience macro captures __FILE__ and __LINE__ automatically. */
#define AURA_PANIC(msg) aura_panic((msg), __FILE__, __LINE__)

#endif /* AURA_PANIC_H */
