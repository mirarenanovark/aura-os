#include <aura/tui.h>
#include <aura/vga.h>
#include <stdarg.h>

/* VGA cell write (for custom layouts) */
void tui_putc_at(int x, int y, char c, uint8_t color) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;
    VGA_MEMORY[y * VGA_WIDTH + x] = (uint16_t)((uint16_t)(unsigned char)c | ((uint16_t)color << 8));
}

void tui_puts_at(int x, int y, const char *str, uint8_t color) {
    while (*str && x < VGA_WIDTH) {
        tui_putc_at(x++, y, *str++, color);
    }
}

void tui_printf_at(int x, int y, uint8_t color, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buf[81];
    int len = 0;
    
    for (const char *p = fmt; *p && len < 80; p++) {
        if (*p != '%') {
            buf[len++] = *p;
            continue;
        }
        
        p++;
        switch (*p) {
        case 's': {
            const char *s = va_arg(args, const char *);
            if (s) {
                while (*s && len < 80) buf[len++] = *s++;
            }
            break;
        }
        case 'd': {
            int val = va_arg(args, int);
            char tmp[12];
            int i = 0;
            if (val < 0) { buf[len++] = '-'; val = -val; }
            do { tmp[i++] = '0' + (val % 10); val /= 10; } while (val && i < 11);
            while (i > 0 && len < 80) buf[len++] = tmp[--i];
            break;
        }
        case 'u': {
            unsigned int val = va_arg(args, unsigned int);
            char tmp[12];
            int i = 0;
            do { tmp[i++] = '0' + (val % 10); val /= 10; } while (val && i < 11);
            while (i > 0 && len < 80) buf[len++] = tmp[--i];
            break;
        }
        case '%':
            buf[len++] = '%';
            break;
        default:
            buf[len++] = '%';
            buf[len++] = *p;
            break;
        }
    }
    buf[len] = '\0';
    
    tui_puts_at(x, y, buf, color);
    va_end(args);
}

void tui_draw_box(int x, int y, int w, int h, uint8_t color, const char *title, int double_border) {
    if (w < 2 || h < 2) return;
    
    unsigned char tl, tr, bl, br, h_line, v_line;
    
    if (double_border) {
        tl = TUI_BOX_DOUBLE_TL;
        tr = TUI_BOX_DOUBLE_TR;
        bl = TUI_BOX_DOUBLE_BL;
        br = TUI_BOX_DOUBLE_BR;
        h_line = TUI_BOX_DOUBLE_H;
        v_line = TUI_BOX_DOUBLE_V;
    } else {
        tl = TUI_BOX_SINGLE_TL;
        tr = TUI_BOX_SINGLE_TR;
        bl = TUI_BOX_SINGLE_BL;
        br = TUI_BOX_SINGLE_BR;
        h_line = TUI_BOX_SINGLE_H;
        v_line = TUI_BOX_SINGLE_V;
    }
    
    /* Top border */
    tui_putc_at(x, y, (char)tl, color);
    for (int i = 1; i < w - 1; i++) {
        tui_putc_at(x + i, y, (char)h_line, color);
    }
    tui_putc_at(x + w - 1, y, (char)tr, color);
    
    /* Title */
    if (title) {
        int title_len = 0;
        const char *t = title;
        while (*t++) title_len++;
        
        int title_x = x + 2;
        tui_putc_at(title_x++, y, ' ', color);
        for (int i = 0; i < title_len && title_x < x + w - 1; i++) {
            tui_putc_at(title_x++, y, title[i], color);
        }
        tui_putc_at(title_x, y, ' ', color);
    }
    
    /* Middle rows */
    for (int j = 1; j < h - 1; j++) {
        tui_putc_at(x, y + j, (char)v_line, color);
        for (int i = 1; i < w - 1; i++) {
            tui_putc_at(x + i, y + j, ' ', color);
        }
        tui_putc_at(x + w - 1, y + j, (char)v_line, color);
    }
    
    /* Bottom border */
    tui_putc_at(x, y + h - 1, (char)bl, color);
    for (int i = 1; i < w - 1; i++) {
        tui_putc_at(x + i, y + h - 1, (char)h_line, color);
    }
    tui_putc_at(x + w - 1, y + h - 1, (char)br, color);
}

void tui_draw_bar(int x, int y, int width, uint32_t val, uint32_t max, uint8_t fg_fill, uint8_t fg_empty) {
    if (width <= 0 || max == 0) return;
    
    uint32_t filled = (val * (uint32_t)width) / max;
    if (filled > (uint32_t)width) filled = (uint32_t)width;
    
    for (int i = 0; i < width; i++) {
        if (i < (int)filled) {
            tui_putc_at(x + i, y, (char)TUI_BAR_FULL, fg_fill);
        } else {
            tui_putc_at(x + i, y, (char)TUI_BAR_FULL, fg_empty);
        }
    }
}

void tui_draw_header(const char *title, const char *subtitle) {
    uint8_t color = 0x1F; /* White on blue (inverted look) */
    
    for (int x = 0; x < VGA_WIDTH; x++) {
        tui_putc_at(x, 0, ' ', color);
    }
    
    tui_puts_at(1, 0, title, color);
    
    if (subtitle) {
        int sub_len = 0;
        const char *s = subtitle;
        while (*s++) sub_len++;
        tui_puts_at(VGA_WIDTH - sub_len - 1, 0, subtitle, color);
    }
}

void tui_draw_footer(const char *hints) {
    uint8_t color = 0x1F; /* White on blue */
    
    for (int x = 0; x < VGA_WIDTH; x++) {
        tui_putc_at(x, VGA_HEIGHT - 1, ' ', color);
    }
    
    tui_puts_at(1, VGA_HEIGHT - 1, hints, color);
}
