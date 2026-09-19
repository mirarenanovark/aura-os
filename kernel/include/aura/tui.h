#ifndef AURA_TUI_H
#define AURA_TUI_H

#include <stdint.h>

/* CP437 Box Drawing Characters */
#define TUI_BOX_SINGLE_TL   0xDA  /* ┌ */
#define TUI_BOX_SINGLE_H    0xC4  /* ─ */
#define TUI_BOX_SINGLE_TR   0xBF  /* ┐ */
#define TUI_BOX_SINGLE_V    0xB3  /* │ */
#define TUI_BOX_SINGLE_BL   0xC0  /* └ */
#define TUI_BOX_SINGLE_BR   0xD9  /* ┘ */

#define TUI_BOX_DOUBLE_TL   0xC9  /* ╔ */
#define TUI_BOX_DOUBLE_H    0xCD  /* ═ */
#define TUI_BOX_DOUBLE_TR   0xBB  /* ╗ */
#define TUI_BOX_DOUBLE_V    0xBA  /* ║ */
#define TUI_BOX_DOUBLE_BL   0xC8  /* ╚ */
#define TUI_BOX_DOUBLE_BR   0xBC  /* ╝ */

/* Progress Bar Characters */
#define TUI_BAR_FULL        219   /* █ */
#define TUI_BAR_75          178   /* ▓ */
#define TUI_BAR_50          177   /* ▒ */
#define TUI_BAR_25          176   /* ░ */

/* Draw a box with optional title and border style */
void tui_draw_box(int x, int y, int w, int h, uint8_t color, const char *title, int double_border);

/* Draw a horizontal progress bar */
void tui_draw_bar(int x, int y, int width, uint32_t val, uint32_t max, uint8_t fg_fill, uint8_t fg_empty);

/* Draw a full-width header bar (inverted colors) */
void tui_draw_header(const char *title, const char *subtitle);

/* Draw a full-width footer bar with key hints */
void tui_draw_footer(const char *hints);

/* Low-level VGA cell write (for custom layouts) */
void tui_putc_at(int x, int y, char c, uint8_t color);
void tui_puts_at(int x, int y, const char *str, uint8_t color);
void tui_printf_at(int x, int y, uint8_t color, const char *fmt, ...);

#endif /* AURA_TUI_H */
