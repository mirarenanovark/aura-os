/**
 * @file        kernel/drivers/vga.c
 * @layer       STAGE_4_SERVICES_DRIVERS
 * @component   DRIVER_VGA_CONSOLE
 * @contract    PRD-07-boot-platform
 * @description Standard 80x25 text mode driver writing directly to physical 0xB8000.
 *              Maintains row/col cursor state, handles automatic vertical scrolling,
 *              tab expansion, newline wraps, and supports integer/string formatting.
 *
 * @connects
 *              - Upstream:   kernel/main.c:kernel_main, kernel/core/panic.c, kernel/drivers/tui.c
 *              - Downstream: MMIO buffer 0xB8000 (VGA text video memory)
 *              - Hardware:   VGA display controller
 *
 * @flow        [AURA_FLOW: VGA_OUTPUT]
 *              1. vga_init(): Sets default light-grey on black, clears video RAM.
 *              2. vga_putc(): Writes character + attribute into VGA_MEMORY[row * 80 + col].
 *              3. vga_scroll(): Shifts lines 1..24 up by one line, blanks bottom row.
 */

#include <aura/vga.h>

/* Cursor position tracking */
static size_t vga_row = 0;
static size_t vga_col = 0;
static uint8_t vga_color = 0;

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) {
    return (uint8_t)((bg << 4) | (fg & 0x0F));
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)((uint16_t)uc | ((uint16_t)color << 8));
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = vga_entry_color(fg, bg);
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', vga_color);
        }
    }
    vga_row = 0;
    vga_col = 0;
}

void vga_init(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
}

static void vga_scroll(void) {
    /* Move every line up by one */
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }
    /* Clear the last line */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color);
    }
    vga_row = VGA_HEIGHT - 1;
}

static void vga_newline(void) {
    vga_col = 0;
    vga_row++;
    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_newline();
        return;
    }

    VGA_MEMORY[vga_row * VGA_WIDTH + vga_col] = vga_entry((unsigned char)c, vga_color);
    vga_col++;

    if (vga_col >= VGA_WIDTH) {
        vga_newline();
    }
}

void vga_puts(const char *str) {
    while (*str) {
        vga_putc(*str++);
    }
}

/* Minimal freestanding printf for VGA output */
#include <stdarg.h>

void vga_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            vga_putc(*p);
            continue;
        }

        p++;
        switch (*p) {
        case 's': {
            const char *s = va_arg(args, const char *);
            vga_puts(s ? s : "(null)");
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            vga_putc(c);
            break;
        }
        case 'd': {
            int val = va_arg(args, int);
            char buf[12];
            int i = 0;
            if (val < 0) { vga_putc('-'); val = -val; }
            do { buf[i++] = '0' + (val % 10); val /= 10; } while (val);
            while (i > 0) vga_putc(buf[--i]);
            break;
        }
        case 'u': {
            unsigned int val = va_arg(args, unsigned int);
            char buf[12];
            int i = 0;
            do { buf[i++] = '0' + (val % 10); val /= 10; } while (val);
            while (i > 0) vga_putc(buf[--i]);
            break;
        }
        case 'x': {
            unsigned int val = va_arg(args, unsigned int);
            const char hex[] = "0123456789abcdef";
            char buf[9];
            int i = 0;
            do { buf[i++] = hex[val & 0xF]; val >>= 4; } while (val);
            while (i > 0) vga_putc(buf[--i]);
            break;
        }
        case '%':
            vga_putc('%');
            break;
        default:
            vga_putc('%');
            vga_putc(*p);
            break;
        }
    }

    va_end(args);
}
