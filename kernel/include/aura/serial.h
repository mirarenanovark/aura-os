/*
 * AuraOS - 16550 UART serial driver (COM1 and friends).
 *
 * Freestanding, no stdlib. Polling only: no interrupts, no DMA.
 * 8N1 framing, FIFO enabled, divisor latched via DLAB.
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

#endif /* AURA_SERIAL_H */
