#include <aura/serial.h>
#include <aura/gdt.h>
#include <aura/idt.h>
#include <aura/pit.h>
#include <aura/pmm.h>

#define KERNEL_MULTIBOOT_MAGIC 0x36D76289

void kernel_main(uint32_t multiboot_info_addr, uint32_t multiboot_magic)
{
    serial_init(COM1, 115200);

    serial_puts(COM1, "\n");
    serial_puts(COM1, "========================================\n");
    serial_puts(COM1, "  AuraOS 0.1.0 - booting...\n");
    serial_puts(COM1, "========================================\n");

    if (multiboot_magic != KERNEL_MULTIBOOT_MAGIC) {
        serial_printf(COM1, "[WARN] Bad multiboot2 magic: 0x%x (expected 0x%x)\n",
                      multiboot_magic, KERNEL_MULTIBOOT_MAGIC);
    } else {
        serial_puts(COM1, "[OK] Multiboot2 magic verified\n");
        serial_printf(COM1, "[OK] Multiboot2 info at 0x%x\n", multiboot_info_addr);
    }

    serial_puts(COM1, "[..] Initializing GDT\n");
    gdt_init();
    serial_puts(COM1, "[OK] GDT loaded (kcode=0x08 kdata=0x10 ucode=0x18 udata=0x20 tss=0x28)\n");

    serial_puts(COM1, "[..] Installing IDT and remapping PIC\n");
    isr_install();
    serial_puts(COM1, "[OK] IDT loaded (256 gates, PIC remapped 0x20/0x28)\n");

    serial_puts(COM1, "[..] Initializing PIT at 1000 Hz\n");
    pit_init();
    serial_puts(COM1, "[OK] PIT channel 0 configured (divisor 1193)\n");

    serial_puts(COM1, "[..] Initializing PMM (64MB @ 0x100000, bitmap @ 0x20000)\n");
    pmm_init(0x4000000, 0x20000);
    serial_printf(COM1, "[OK] PMM ready: %u KB free / %u KB total\n",
                  (uint32_t)(pmm_get_free_memory() / 1024),
                  (uint32_t)(pmm_get_total_memory() / 1024));

    serial_puts(COM1, "[OK] Enabling interrupts\n");
    __asm__ volatile("sti");

    uint64_t start_ticks = pit_get_ticks();
    while (pit_get_ticks() < start_ticks + 100) {
        __asm__ volatile("hlt");
    }
    serial_printf(COM1, "[OK] Timer alive: %u ticks in ~100ms\n",
                  (uint32_t)(pit_get_ticks() - start_ticks));

    serial_puts(COM1, "\n");
    serial_puts(COM1, "AuraOS kernel initialized. Halting.\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
