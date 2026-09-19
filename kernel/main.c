/**
 * @file        kernel/main.c
 * @layer       ALL (Kernel Core Orchestrator)
 * @component   KERNEL_ORCHESTRATOR
 * @contract    MASTER_SPEC / PRD-07-boot-platform
 * @description Central kernel entry point coordinating Phase 1 bring-up:
 *              VGA text mode, serial COM1 diagnostics, Multiboot2 info parsing,
 *              GDT & 64-bit TSS, IDT 256 gates with PIC remapping, PIT 1000Hz timer,
 *              PMM bitmap frame allocator, dynamic free-list heap, telemetry monitor,
 *              timer delivery verification, and interactive TUI dashboard.
 *
 * @connects
 *              - Upstream:   boot/entry64.S (long_mode_start)
 *              - Downstream: vga, serial, multiboot2, gdt, idt, pit, pmm, heap, sysmon, dashboard
 *              - Hardware:   VGA 0xB8000, COM1 0x3F8, 8259 PIC 0x20/0x28, 8254 PIT 0x40/0x43
 *
 * @flow        [AURA_FLOW: KERNEL_INIT]
 *              1. Initialize VGA text mode console (immediate monitor feedback).
 *              2. Initialize 16550 UART COM1 (115200 baud 8N1 serial logging).
 *              3. Parse Multiboot2 tags (RAM limits, memory map, linear GOP framebuffer).
 *              4. Install 5-segment GDT + 64-bit TSS.
 *              5. Remap 8259 PIC to 0x20/0x28 and register 256 IDT gates.
 *              6. Configure PIT channel 0 to 1000Hz mode 3 square wave.
 *              7. Initialize PMM bitmap frame allocator for available physical RAM.
 *              8. Initialize kernel dynamic heap (free-list allocator).
 *              9. Initialize telemetry system monitor hooked to PIT timer ticks.
 *              10. Enable CPU interrupts ("sti") and verify timer tick delivery.
 *              11. Wait for 1-second sample window and render btop-style dashboard.
 *              12. Enter low-power idle loop (sysmon_set_idle + hlt).
 */

#include <stdint.h>
#include <aura/serial.h>
#include <aura/gdt.h>
#include <aura/idt.h>
#include <aura/pit.h>
#include <aura/pmm.h>
#include <aura/multiboot2.h>
#include <aura/vga.h>
#include <aura/heap.h>
#include <aura/sysmon.h>
#include <aura/dashboard.h>

static struct aura_boot_info boot_info;

void kernel_main(uint64_t mbi_addr, uint64_t magic) {
    // [AURA_FLOW: KERNEL_INIT] Step 1: Initialize VGA text console
    // [AURA_CONNECTS: DRIVER_VGA_CONSOLE -> vga_init]
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_printf("========================================\n");
    vga_printf("    AuraOS Kernel v0.1.0-alpha\n");
    vga_printf("  Clean. Simple. Small. Fast. Direct.\n");
    vga_printf("========================================\n\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    // [AURA_FLOW: KERNEL_INIT] Step 2: Initialize COM1 UART serial logger
    // [AURA_CONNECTS: SERIAL_UART16550 -> serial_init]
    serial_init(COM1, 115200);
    serial_printf(COM1, "\n\n========================================\n");
    serial_printf(COM1, "    AuraOS Kernel v0.1.0-alpha (Phase 1)\n");
    serial_printf(COM1, "  Clean. Simple. Small. Fast. Direct.\n");
    serial_printf(COM1, "========================================\n\n");

    // [AURA_FLOW: KERNEL_INIT] Step 3: Parse Multiboot2 information tags (Memory map + Framebuffer)
    // [AURA_CONNECTS: BOOT_MULTIBOOT2 -> multiboot2_parse]
    if (multiboot2_parse((uint32_t)magic, (uintptr_t)mbi_addr, &boot_info) == 0) {
        serial_printf(COM1, "[OK] Multiboot2 tags parsed successfully\n");
        vga_printf("[OK] Multiboot2 tags parsed successfully\n");
        if (boot_info.total_memory_bytes > 0) {
            serial_printf(COM1, "     Total RAM: %u MB (Available: %u MB)\n",
                          (uint32_t)(boot_info.total_memory_bytes / (1024 * 1024)),
                          (uint32_t)(boot_info.available_memory_bytes / (1024 * 1024)));
            vga_printf("     Total RAM: %u MB (Available: %u MB)\n",
                       (uint32_t)(boot_info.total_memory_bytes / (1024 * 1024)),
                       (uint32_t)(boot_info.available_memory_bytes / (1024 * 1024)));
        }
        if (boot_info.has_framebuffer) {
            serial_printf(COM1, "     Linear FB: %ux%u @ %u bpp (Base: %p)\n",
                          boot_info.fb_width, boot_info.fb_height,
                          (uint32_t)boot_info.fb_bpp, (void*)(uintptr_t)boot_info.fb_addr);
            vga_printf("     Linear FB: %ux%u @ %u bpp\n",
                       boot_info.fb_width, boot_info.fb_height, (uint32_t)boot_info.fb_bpp);
        }
    } else {
        serial_printf(COM1, "[WARN] Multiboot2 magic mismatch or tags missing\n");
        vga_printf("[WARN] Multiboot2 tags missing\n");
    }

    // [AURA_FLOW: KERNEL_INIT] Step 4: Initialize Global Descriptor Table & 64-bit TSS
    // [AURA_CONNECTS: ARCH_GDT -> gdt_init]
    gdt_init();
    serial_printf(COM1, "[OK] GDT loaded (kcode=0x08 kdata=0x10 ucode=0x18 udata=0x20 tss=0x28)\n");
    vga_printf("[OK] GDT loaded (Ring 0 & Ring 3 segments)\n");

    // [AURA_FLOW: KERNEL_INIT] Step 5: Initialize Interrupt Descriptor Table & remap 8259 PIC
    // [AURA_CONNECTS: ARCH_IDT -> isr_install]
    isr_install();
    serial_printf(COM1, "[OK] IDT loaded (256 gates, PIC remapped 0x20/0x28)\n");
    vga_printf("[OK] IDT loaded (256 gates, PIC remapped)\n");

    // [AURA_FLOW: KERNEL_INIT] Step 6: Initialize Programmable Interval Timer at 1000 Hz
    // [AURA_CONNECTS: ARCH_PIT_8254 -> pit_init]
    pit_init();
    serial_printf(COM1, "[OK] PIT channel 0 configured (1000 Hz heartbeat)\n");
    vga_printf("[OK] PIT timer active (1000 Hz)\n");

    // [AURA_FLOW: KERNEL_INIT] Step 7: Initialize Physical Memory Manager
    // [AURA_CONNECTS: PMM_BITMAP_ALLOCATOR -> pmm_init]
    uint64_t ram_size = boot_info.total_memory_bytes ? boot_info.total_memory_bytes : (64ULL * 1024 * 1024);
    pmm_init((uintptr_t)ram_size, 0x20000);
    serial_printf(COM1, "[OK] PMM ready: %u KB free / %u KB total\n",
                  (uint32_t)(pmm_get_free_memory() / 1024),
                  (uint32_t)(pmm_get_total_memory() / 1024));
    vga_printf("[OK] PMM ready: %u MB free / %u MB total\n",
               (uint32_t)(pmm_get_free_memory() / (1024 * 1024)),
               (uint32_t)(pmm_get_total_memory() / (1024 * 1024)));

    // [AURA_FLOW: KERNEL_INIT] Step 8: Initialize dynamic kernel heap
    // [AURA_CONNECTS: KERNEL_HEAP_FREELIST -> heap_init]
    heap_init(0);
    serial_printf(COM1, "[OK] Heap initialized (identity mapped)\n");
    vga_printf("[OK] Heap initialized\n");

    // [AURA_FLOW: KERNEL_INIT] Step 9: Initialize sysmon (must be after PIT + PMM)
    // [AURA_CONNECTS: TELEMETRY_SYSMON -> sysmon_init]
    sysmon_init();
    serial_printf(COM1, "[OK] System monitor started\n");
    vga_printf("[OK] System monitor started\n");

    // [AURA_FLOW: KERNEL_INIT] Step 10: Enable CPU interrupts & verify delivery
    __asm__ __volatile__("sti");

    uint64_t start_ticks = pit_get_ticks();
    while (pit_get_ticks() - start_ticks < 50) {
        __asm__ __volatile__("pause");
    }
    serial_printf(COM1, "[OK] Timer heartbeat verified: %u ticks elapsed\n",
                  (uint32_t)(pit_get_ticks() - start_ticks));
    vga_printf("[OK] Timer verified: 50 ticks elapsed\n");

    serial_printf(COM1, "\n>>> AuraOS Stage 1 Boot Complete. System ready. <<<\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_printf("\n>>> AuraOS Stage 1 Boot Complete. System Ready. <<<\n");

    // [AURA_FLOW: KERNEL_INIT] Step 11: Wait for initial telemetry window (1100ms)
    uint64_t wait_ticks = pit_get_ticks();
    while (pit_get_ticks() - wait_ticks < 1100) {
        __asm__ __volatile__("pause");
    }

    // [AURA_FLOW: KERNEL_INIT] Step 12: Render btop-style monitoring dashboard
    // [AURA_CONNECTS: MONITOR_DASHBOARD -> dashboard_render]
    dashboard_init();
    dashboard_render();

    // [AURA_FLOW: KERNEL_IDLE] Low-power idle loop
    while (1) {
        sysmon_set_idle();
        __asm__ __volatile__("hlt");
    }
}
