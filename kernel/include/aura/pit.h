#ifndef AURA_PIT_H
#define AURA_PIT_H

#include <stdint.h>

#define PIT_TICKS_PER_SECOND 1000
#define PIT_BASE_FREQUENCY   1193182

void pit_init(void);
void pit_set_callback(void (*cb)(void));
uint64_t pit_get_ticks(void);

#endif /* AURA_PIT_H */
