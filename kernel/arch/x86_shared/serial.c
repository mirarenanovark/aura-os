/**
 * @file        kernel/arch/x86_shared/serial.c
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   SERIAL_UART16550
 * @contract    PRD-07-boot-platform
 * @description 16550 UART serial driver for x86 architectures (i686 & x86_64).
 *              Operates purely via port I/O polling (no IRQ, no DMA).
 *              Configures 8N1 framing, enables 14-byte FIFO trigger, and sets
 *              divisor latch via DLAB. Formats output for remote diagnostics.
 *
 * @connects
 *              - Upstream:   kernel/main.c:kernel_main, kernel/core/panic.c
 *              - Downstream: x86 I/O bus via inline inb/outb assembly
 *              - Hardware:   COM1 base port 0x3F8, COM2 0x2F8
 *
 * @flow        [AURA_FLOW: SERIAL_IO] [AURA_FLOW: SERIAL_RX]
 *              1. serial_init(): Disables UART IRQ, latches DLAB, programs baud divisor.
 *              2. serial_putc(): Polls LSR bit 5 (THRE) until transmitter empty, writes byte.
 *              3. serial_has_char(): Polls LSR bit 0 (DR) for received data.
 *              4. serial_getc(): Non-blocking read of received byte (returns 0 if empty).
 *              5. serial_printf(): Formats integer/string tokens and writes out bytes.
 */

#include <aura/serial.h>

/* Register offsets from the port base. */
#define REG_DATA        0 /* R/W: receive/transmit buffer          */
#define REG_DIV_LO      0 /* R/W: divisor low  (DLAB=1)            */
#define REG_DIV_HI      1 /* R/W: divisor high (DLAB=1)            */
#define REG_IER         1 /* R/W: interrupt enable                 */
#define REG_FCR         2 /* W:   FIFO control                     */
#define REG_LCR         3 /* R/W: line control (frame + DLAB)      */
#define REG_MCR         4 /* R/W: modem control                    */
#define REG_LSR         5 /* R:   line status                      */

/* LSR bits: transmitter holding register empty is bit 5. */
#define LSR_THRE        (1u << 5)

/* LSR bit 0: data ready (DR) — byte received and available in receive buffer. */
#define LSR_DR          (1u << 0)

/* Line control: 8 data bits, no parity, 1 stop, DLAB in bit 7. */
#define LCR_8N1         0x03u
#define LCR_DLAB        (1u << 7)

/* FIFO control: enable FIFO, clear both FIFOs, 14-byte trigger. */
#define FCR_ENABLE      0xC7u

/* Modem control: DTR + RTS + OUT2 (OUT2 gates UART IRQs on PCs). */
#define MCR_DTR_RTS_OUT2 0x0Bu

static inline void outb(uint16_t port, uint8_t val)
{
        __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
        uint8_t v;
        __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
        return v;
}

void serial_init(uint16_t port, uint32_t baud)
{
        uint16_t div;

        if (baud == 0)
                baud = 115200;
        div = (uint16_t)(115200u / baud);

        outb((uint16_t)(port + REG_IER), 0x00);  /* no interrupts yet   */
        outb((uint16_t)(port + REG_LCR), LCR_DLAB);
        outb((uint16_t)(port + REG_DIV_LO), (uint8_t)(div & 0xFFu));
        outb((uint16_t)(port + REG_DIV_HI), (uint8_t)(div >> 8));
        outb((uint16_t)(port + REG_LCR), LCR_8N1); /* 8N1, DLAB off    */
        outb((uint16_t)(port + REG_FCR), FCR_ENABLE);
        outb((uint16_t)(port + REG_MCR), MCR_DTR_RTS_OUT2);
}

void serial_putc(uint16_t port, char c)
{
        while ((inb((uint16_t)(port + REG_LSR)) & LSR_THRE) == 0)
                ;
        outb((uint16_t)(port + REG_DATA), (uint8_t)c);
}

/** [AURA_FLOW: SERIAL_RX]
 * Check if a byte is waiting in the receive buffer.
 * Returns non-zero (true) if data ready (LSR bit 0 set), zero (false) if empty. */
int serial_has_char(uint16_t port)
{
        /* [AURA_CONNECTS: UART16550 -> CPU_POLL] */
        return (inb((uint16_t)(port + REG_LSR)) & LSR_DR) != 0;
}

/** [AURA_FLOW: SERIAL_RX]
 * Non-blocking read: returns the received byte, or 0 if no data available.
 * Caller must check serial_has_char() first for reliable detection. */
char serial_getc(uint16_t port)
{
        /* [AURA_CONNECTS: UART16550 -> CPU_POLL] */
        if (!serial_has_char(port))
                return 0;
        return (char)inb((uint16_t)(port + REG_DATA));
}

void serial_puts(uint16_t port, const char *s)
{
        while (*s != '\0')
                serial_putc(port, *s++);
}

static const char hex_digits[] = "0123456789abcdef";

/* Print `v` in base 16 (hex=true) or 10. Zero returns "0"; digits are
 * emitted most-significant first. */
static void print_number(uint16_t port, uint32_t v, int hex)
{
        char buf[8]; /* max: 8 hex digits fits a 32-bit value; 10 decimal digits buffered via repeated pass below */

        (void)buf;
        if (hex) {
                char tmp[8];
                int i = 0;

                if (v == 0) {
                        serial_putc(port, '0');
                        return;
                }
                while (v != 0) {
                        tmp[i++] = hex_digits[v & 0xFu];
                        v >>= 4;
                }
                while (i > 0)
                        serial_putc(port, tmp[--i]);
        } else {
                char tmp[10];
                int i = 0;

                if (v == 0) {
                        serial_putc(port, '0');
                        return;
                }
                while (v != 0) {
                        tmp[i++] = (char)('0' + (v % 10u));
                        v /= 10u;
                }
                while (i > 0)
                        serial_putc(port, tmp[--i]);
        }
}

static void print_signed(uint16_t port, int32_t v)
{
        uint32_t u;

        if (v < 0) {
                serial_putc(port, '-');
                u = (uint32_t)(-(int64_t)v); /* avoid UB on INT32_MIN */
        } else {
                u = (uint32_t)v;
        }
        print_number(port, u, 0);
}

static void print_string(uint16_t port, const char *s)
{
        if (s == 0) {
                serial_puts(port, "(null)");
                return;
        }
        serial_puts(port, s);
}

static void print_hex_pointer(uint16_t port, uintptr_t p)
{
        char tmp[(sizeof(uintptr_t) * 2) + 1];
        int i = 0;

        serial_puts(port, "0x");
        if (p == 0) {
                serial_putc(port, '0');
                return;
        }
        while (p != 0) {
                tmp[i++] = hex_digits[p & 0xFu];
                p >>= 4;
        }
        while (i > 0)
                serial_putc(port, tmp[--i]);
}

void serial_printf(uint16_t port, const char *fmt, ...)
{
        __builtin_va_list ap;

        if (fmt == 0)
                return;

        __builtin_va_start(ap, fmt);
        while (*fmt != '\0') {
                char c = *fmt++;

                if (c != '%') {
                        serial_putc(port, c);
                        continue;
                }
                c = *fmt++;
                switch (c) {
                case 's': {
                        const char *s = __builtin_va_arg(ap, const char *);
                        print_string(port, s);
                        break;
                }
                case 'd': {
                        int32_t v = __builtin_va_arg(ap, int32_t);
                        print_signed(port, v);
                        break;
                }
                case 'u': {
                        uint32_t v = __builtin_va_arg(ap, uint32_t);
                        print_number(port, v, 0);
                        break;
                }
                case 'x': {
                        uint32_t v = __builtin_va_arg(ap, uint32_t);
                        print_number(port, v, 1);
                        break;
                }
                case 'p': {
                        uintptr_t v = (uintptr_t)__builtin_va_arg(ap, void *);
                        print_hex_pointer(port, v);
                        break;
                }
                case 'c': {
                        int v = __builtin_va_arg(ap, int);
                        serial_putc(port, (char)v);
                        break;
                }
                case '%':
                        serial_putc(port, '%');
                        break;
                case '\0':
                        serial_putc(port, '%'); /* trailing lone '%' */
                        --fmt;                  /* keep loop ending   */
                        return;
                default:
                        serial_putc(port, '%');
                        serial_putc(port, c);
                        break;
                }
        }
        __builtin_va_end(ap);
}
