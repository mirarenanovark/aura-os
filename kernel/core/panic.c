/*
 * AuraOS - Kernel panic handler.
 *
 * Prints a PANIC banner (white on red) to both VGA text console and
 * COM1 serial, then halts the CPU with cli; hlt in an infinite loop.
 */

#include <aura/panic.h>
#include <aura/vga.h>
#include <aura/serial.h>

void aura_panic(const char *msg, const char *file, uint32_t line) {
    /* Disable interrupts immediately */
    __asm__ volatile("cli");

    /* VGA: white on red, clear screen, print banner */
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_clear();
    vga_puts("=== KERNEL PANIC ===\n\n");
    vga_puts(msg);
    vga_putc('\n');
    if (file) {
        vga_puts("  at ");
        vga_puts(file);
        vga_puts(":");
        /* Inline itoa for line number (no printf dependency in panic path) */
        char buf[12];
        int i = 0;
        uint32_t val = line;
        do { buf[i++] = '0' + (val % 10); val /= 10; } while (val);
        while (i > 0) vga_putc(buf[--i]);
        vga_putc('\n');
    }
    vga_puts("\nSystem halted.");

    /* Serial: same message */
    serial_puts(COM1, "\r\n=== KERNEL PANIC ===\r\n\r\n");
    serial_puts(COM1, msg);
    serial_puts(COM1, "\r\n");
    if (file) {
        serial_puts(COM1, "  at ");
        serial_puts(COM1, file);
        serial_putc(COM1, ':');
        serial_printf(COM1, "%u", line);
        serial_puts(COM1, "\r\n");
    }
    serial_puts(COM1, "\r\nSystem halted.\r\n");

    /* Halt forever */
    for (;;) {
        __asm__ volatile("hlt");
    }
}
