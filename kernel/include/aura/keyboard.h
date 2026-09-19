#ifndef AURA_KEYBOARD_H
#define AURA_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize PS/2 Keyboard driver on IRQ1 (vector 33).
 */
void keyboard_init(void);

/**
 * Non-blocking check for next character from keyboard buffer.
 * Returns ASCII char (> 0), or 0 if buffer is empty.
 */
char keyboard_getchar(void);

/**
 * Check if keyboard has unread keys.
 */
bool keyboard_has_key(void);

#endif /* AURA_KEYBOARD_H */
