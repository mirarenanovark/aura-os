/*
 * AuraOS - Kernel panic handler.
 *
 * Halts the CPU after printing a diagnostic to both VGA and COM1.
 */

#ifndef AURA_PANIC_H
#define AURA_PANIC_H

#include <stdint.h>

/* Kernel panic: print message with file/line to VGA+serial, then halt. */
void aura_panic(const char *msg, const char *file, uint32_t line);

/* Convenience macro captures __FILE__ and __LINE__ automatically. */
#define AURA_PANIC(msg) aura_panic((msg), __FILE__, __LINE__)

#endif /* AURA_PANIC_H */
