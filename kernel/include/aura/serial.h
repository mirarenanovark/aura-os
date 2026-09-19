/**
 * @file        kernel/include/aura/serial.h
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   SERIAL_UART16550
 * @contract    PRD-07-boot-platform
 * @description Header for the 16550 UART serial driver (COM1 0x3F8).
 *              Polled character/string transmission and non-blocking receive, formatted printing.
 *
 * @connects
 *              - Upstream:   kernel/main.c, kernel/core/panic.c, kernel/core/multiboot2.c
 *              - Downstream: kernel/arch/x86_shared/serial.c
 *              - Hardware:   I/O Ports 0x3F8 - 0x3FF
 */

#ifndef AURA_SERIAL_H
#define AURA_SERIAL_H

#include <stdint.h>

/* COM1 base port (industry standard). COM2/3/4: 0x2F8, 0x3E8, 0x2E8. */
#define COM1 0x3F8u

/* Initialize the UART at `port` for 8N1 at `baud` (divisor from 115200 base).
 * If baud is 0, defaults to 115200. */
void serial_init(uint16_t port, uint32_t baud);

/* Write one character, polling the line status register until the
 * transmitter is ready. Expands '\n' to '\r\n' for terminals. */
void serial_putc(uint16_t port, char c);

/* Write a NUL-terminated string. */
void serial_puts(uint16_t port, const char *s);

/* Minimal printf over serial. Supported: %s %d %u %x %c %p and %%.
 * No width, precision, or length modifiers. */
void serial_printf(uint16_t port, const char *fmt, ...);

/* Non-blocking check: returns non-zero if a byte is waiting in the receive
 * buffer (LSR bit 0 — Data Ready), zero if empty. */
int serial_has_char(uint16_t port);

/* Non-blocking read: returns the received byte, or 0 if no data available.
 * Caller should check serial_has_char() first. */
char serial_getc(uint16_t port);

#endif /* AURA_SERIAL_H */
