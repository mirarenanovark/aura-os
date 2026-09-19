#include <aura/pit.h>
#include <aura/idt.h>
#include <stdint.h>
#include <stddef.h>

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43

/* Channel 0, Access mode lo/hi byte, Mode 3 (square wave), 16-bit binary */
/* Binary: 00 11 011 0 = 0x36 */
#define PIT_MODE_SQUARE_WAVE 0x36

static volatile uint64_t pit_ticks = 0;
static void (*pit_callback)(void) = NULL;

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void pit_irq_handler(registers_t *regs)
{
    (void)regs;
    pit_ticks++;
    if (pit_callback) {
        pit_callback();
    }
}

void pit_init(void)
{
    uint32_t divisor = PIT_BASE_FREQUENCY / PIT_TICKS_PER_SECOND; /* ~1193 */

    outb(PIT_COMMAND, PIT_MODE_SQUARE_WAVE);
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register handler at vector 32 (IRQ0) */
    isr_register_handler(32, pit_irq_handler);
}

void pit_set_callback(void (*cb)(void))
{
    pit_callback = cb;
}

uint64_t pit_get_ticks(void)
{
    return pit_ticks;
}
