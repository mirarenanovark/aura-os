#include <stdint.h>
#include <aura/serial.h>
#include <aura/gdt.h>
#include <aura/idt.h>
#include <aura/pit.h>
#include <aura/pmm.h>
#include <aura/multiboot2.h>

static struct aura_boot_info boot_info;

void kernel_main(uint64_t mbi_addr, uint64_t magic) {
    /* Initialize COM1 UART immediately for early diagnostics */
    serial_init(COM1, 115200);
    serial_printf(COM1, "\n\n========================================\n");
    serial_printf(COM1, "    AuraOS Kernel v0.1.0-alpha (Phase 1)\n");
    serial_printf(COM1, "  Clean. Simple. Small. Fast. Direct.\n");
    serial_printf(COM1, "========================================\n\n");

    /* Parse Multiboot2 information tags (Memory map + Framebuffer) */
    if (multiboot2_parse((uint32_t)magic, (uintptr_t)mbi_addr, &boot_info) == 0) {
        serial_printf(COM1, "[OK] Multiboot2 tags parsed successfully\n");
        if (boot_info.total_memory_bytes > 0) {
            serial_printf(COM1, "     Total RAM: %u MB (Available: %u MB)\n",
                          (uint32_t)(boot_info.total_memory_bytes / (1024 * 1024)),
                          (uint32_t)(boot_info.available_memory_bytes / (1024 * 1024)));
        }
        if (boot_info.has_framebuffer) {
            serial_printf(COM1, "     Linear FB: %ux%u @ %u bpp (Base: %p)\n",
                          boot_info.fb_width, boot_info.fb_height,
                          (uint32_t)boot_info.fb_bpp, (void*)(uintptr_t)boot_info.fb_addr);
        }
    } else {
        serial_printf(COM1, "[WARN] Multiboot2 magic mismatch or tags missing\n");
    }

    /* Initialize Global Descriptor Table & 64-bit TSS */
    gdt_init();
    serial_printf(COM1, "[OK] GDT loaded (kcode=0x08 kdata=0x10 ucode=0x18 udata=0x20 tss=0x28)\n");

    /* Initialize Interrupt Descriptor Table & remap 8259 PIC */
    isr_install();
    serial_printf(COM1, "[OK] IDT loaded (256 gates, PIC remapped 0x20/0x28)\n");

    /* Initialize Programmable Interval Timer at 1000 Hz */
    pit_init();
    serial_printf(COM1, "[OK] PIT channel 0 configured (1000 Hz heartbeat)\n");

    /* Initialize Physical Memory Manager */
    uint64_t ram_size = boot_info.total_memory_bytes ? boot_info.total_memory_bytes : (64ULL * 1024 * 1024);
    pmm_init((uintptr_t)ram_size, 0x20000);
    serial_printf(COM1, "[OK] PMM ready: %u KB free / %u KB total\n",
                  (uint32_t)(pmm_get_free_memory() / 1024),
                  (uint32_t)(pmm_get_total_memory() / 1024));

    /* Enable CPU interrupts */
    __asm__ __volatile__("sti");

    /* Verify timer interrupt delivery (await 50 ticks ~ 50ms) */
    uint64_t start_ticks = pit_get_ticks();
    while (pit_get_ticks() - start_ticks < 50) {
        __asm__ __volatile__("pause");
    }
    serial_printf(COM1, "[OK] Timer heartbeat verified: %u ticks elapsed\n",
                  (uint32_t)(pit_get_ticks() - start_ticks));

    serial_printf(COM1, "\n>>> AuraOS Stage 1 Boot Complete. System ready. <<<\n\n");

    /* System idle loop */
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
