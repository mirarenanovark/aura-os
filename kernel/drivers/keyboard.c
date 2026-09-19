/**
 * @file        kernel/drivers/keyboard.c
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   PS2_KEYBOARD_DRIVER
 * @description Minimal PS/2 keyboard driver for IRQ1 (vector 33).
 *              Translates scancodes (set 1) to ASCII, stores in a small ring buffer.
 *              Clean. Simple. Small. Fast. Direct.
 */

#include <aura/keyboard.h>
#include <aura/idt.h>
#include <aura/serial.h>
#include <stdint.h>
#include <stddef.h>

#define PS2_DATA_PORT   0x60
#define PS2_STATUS_PORT 0x64

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

#define KBD_BUF_SIZE 32

/* US QWERTY scancode (set 1) -> ASCII, indices 0x00-0x59 */
static const char scancode_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', /* 0x00 - 0x09 */
    '9', '0', '-', '=', '\b',                        /* 0x0A - 0x0E */
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', /* 0x0F - 0x18 */
    'p', '[', ']', '\n', 0,                           /* 0x19 - 0x1D */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 0x1E - 0x27 */
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', /* 0x28 - 0x31 */
    'm', ',', '.', '/', 0, '*',                       /* 0x32 - 0x37 */
    0, ' ', 0,                                         /* 0x38 - 0x3A */
};

/* Simple ring buffer */
static volatile char kbd_buf[KBD_BUF_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

static void keyboard_handler(registers_t *regs) {
    (void)regs;

    uint8_t scancode = inb(PS2_DATA_PORT);

    /* Ignore break codes (bit 7 set) */
    if (scancode & 0x80) return;

    if (scancode < 128) {
        char c = scancode_ascii[scancode];
        if (c) {
            int next = (kbd_head + 1) % KBD_BUF_SIZE;
            if (next != kbd_tail) {  /* Not full */
                kbd_buf[kbd_head] = c;
                kbd_head = next;
            }
        }
    }
}

void keyboard_init(void) {
    kbd_head = 0;
    kbd_tail = 0;
    isr_register_handler(33, keyboard_handler);  /* IRQ1 -> vector 33 */
    serial_printf(COM1, "[OK] PS/2 keyboard driver (IRQ1)\n");
}

char keyboard_getchar(void) {
    if (kbd_tail == kbd_head) return 0;  /* Empty */
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return c;
}

bool keyboard_has_key(void) {
    return kbd_tail != kbd_head;
}
