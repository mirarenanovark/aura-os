#ifndef AURA_MENU_H
#define AURA_MENU_H

#include <stdint.h>

/**
 * Initialize boot menu & interactive prompt.
 */
void menu_init(void);

/**
 * Handle key input and update menu / prompt.
 */
void menu_handle_key(char c);

/**
 * Render the boot screen: system stats on top, command line on bottom.
 */
void menu_render(void);

/**
 * Periodic update for live clock / stats (called from main loop).
 */
void menu_update(void);

/**
 * Add a message to the menu console output log.
 */
void menu_log(const char *msg);

#endif /* AURA_MENU_H */



