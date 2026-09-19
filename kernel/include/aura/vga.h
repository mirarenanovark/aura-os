/**
 * @file        kernel/include/aura/vga.h
 * @layer       STAGE_4_SERVICES_DRIVERS
 * @component   DRIVER_VGA_CONSOLE
 * @contract    PRD-07-boot-platform
 * @description 80x25 standard VGA text mode console interface.
 *              Linear MMIO buffer at 0xB8000, 16-color attributes,
 *              scrolling, clear screen, and formatted printing.
 *
 * @connects
 *              - Upstream:   kernel/main.c, kernel/drivers/tui.c, kernel/core/panic.c
 *              - Downstream: kernel/drivers/vga.c
 *              - Hardware:   VGA memory mapped I/O at physical 0xB8000
 */

#ifndef AURA_VGA_H
#define AURA_VGA_H

#include <stdint.h>
#include <stddef.h>

/* VGA Text Mode (80x25 characters, 2 bytes per cell: ASCII + Color Attribute) */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

/* Standard VGA Colors */
enum vga_color {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
};

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char *str);
void vga_printf(const char *fmt, ...);
void vga_set_color(uint8_t fg, uint8_t bg);

#endif /* AURA_VGA_H */
