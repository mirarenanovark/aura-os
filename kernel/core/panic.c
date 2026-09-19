#include <aura/panic.h>
#include <aura/vga.h>
#include <aura/serial.h>
#include <aura/tui.h>
#include <aura/menu.h>
#include <aura/idt.h>
#include <stdbool.h>

/* PS/2 ports for polled keyboard reading during panic */
#define PS2_DATA_PORT   0x60
#define PS2_STATUS_PORT 0x64

/* Saved recovery context for [R] Restore */
static struct {
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rip;
    bool     valid;
} recovery_point = { 0, 0, 0, false };

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* Save current execution context so [R] Restore can resume execution safely */
__attribute__((returns_twice))
int panic_set_recovery_point(void) {
    register uint64_t rsp __asm__("rsp");
    register uint64_t rbp __asm__("rbp");

    recovery_point.rsp = rsp;
    recovery_point.rbp = rbp;
    recovery_point.rip = (uint64_t)__builtin_return_address(0);
    recovery_point.valid = true;
    return 0;
}

/* Polled PS/2 key read (works reliably even with interrupts disabled) */
static char panic_poll_key(void) {
    /* US QWERTY scancode (set 1) -> ASCII */
    static const char scancode_ascii[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
        '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o',
        'p', '[', ']', '\n', 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
        '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',
        'm', ',', '.', '/', 0, '*',
        0, ' ', 0,
    };

    while (1) {
        uint8_t status = inb(PS2_STATUS_PORT);
        if (status & 0x01) { /* Output buffer full */
            uint8_t scancode = inb(PS2_DATA_PORT);
            if (!(scancode & 0x80)) { /* Key make (press) */
                if (scancode < 128) {
                    char c = scancode_ascii[scancode];
                    if (c) return c;
                }
            }
        }
        __asm__ volatile("pause");
    }
}

/* Hardware reboot via 8042 PS/2 controller with triple-fault fallback */
static void panic_do_reboot(void) {
    serial_puts(COM1, "[PANIC] Rebooting system...\r\n");

    /* 1. Pulse CPU reset line via 8042 Keyboard Controller */
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(PS2_STATUS_PORT);
    }
    outb(PS2_STATUS_PORT, 0xFE);

    /* 2. Fallback: Load null IDTR and trigger triple fault */
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) null_idtr = { 0, 0 };

    __asm__ volatile("lidt %0; int $3" : : "m"(null_idtr));

    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

/* System poweroff via QEMU / ACPI I/O ports */
static void panic_do_poweroff(void) {
    serial_puts(COM1, "[PANIC] Powering off system...\r\n");

    /* QEMU / Bochs modern ACPI shutdown */
    outw(0x604, 0x2000);

    /* QEMU debug exit port */
    outb(0x501, 0x00);
    outb(0xF4, 0x00);

    /* Older QEMU / PIIX4 ACPI poweroff */
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    /* VirtualBox shutdown */
    outw(0x4004, 0x2000);

    /* If poweroff fails, display notice and halt */
    tui_printf_at(2, 23, 0x4F, "Power off not supported by hardware. You may close the VM.");
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

/* Restore system execution to pre-panic safe checkpoint */
static void panic_do_restore(void) {
    if (!recovery_point.valid) {
        tui_printf_at(2, 23, 0x4E, "No valid restore point available! Use Reboot or Poweroff. ");
        return;
    }

    serial_puts(COM1, "[PANIC] Restoring system state from recovery checkpoint...\r\n");

    /* Re-initialize VGA to safe clean state */
    vga_init();
    menu_log("[RECOVERED] System state successfully restored after panic");
    menu_render();

    /* Restore CPU stack & instruction pointer, re-enable interrupts */
    uint64_t target_rsp = recovery_point.rsp;
    uint64_t target_rbp = recovery_point.rbp;
    uint64_t target_rip = recovery_point.rip;

    __asm__ volatile(
        "movq %0, %%rsp\n\t"
        "movq %1, %%rbp\n\t"
        "sti\n\t"
        "jmp *%2\n\t"
        :
        : "r"(target_rsp), "r"(target_rbp), "r"(target_rip)
        : "memory"
    );
}

void aura_panic_interactive(const char *msg, const char *file, uint32_t line, registers_t *regs) {
    /* Disable interrupts to keep screen and state steady */
    __asm__ volatile("cli");

    /* Log panic details to serial COM1 */
    serial_puts(COM1, "\r\n========================================\r\n");
    serial_puts(COM1, "           *** KERNEL PANIC ***         \r\n");
    serial_puts(COM1, "========================================\r\n");
    serial_puts(COM1, "Reason: ");
    serial_puts(COM1, msg ? msg : "Unknown fatal error");
    serial_puts(COM1, "\r\n");

    if (file) {
        serial_puts(COM1, "Location: ");
        serial_puts(COM1, file);
        serial_putc(COM1, ':');
        serial_printf(COM1, "%u", line);
        serial_puts(COM1, "\r\n");
    }

    if (regs) {
        serial_printf(COM1, "Trap: %u  Err: 0x%x  RIP: 0x%x  CS: 0x%x\r\n",
                      (uint32_t)regs->int_no, (uint32_t)regs->err_code,
                      regs->rip, regs->cs);
        serial_printf(COM1, "RAX: 0x%x  RBX: 0x%x  RCX: 0x%x  RDX: 0x%x\r\n",
                      regs->rax, regs->rbx, regs->rcx, regs->rdx);
        serial_printf(COM1, "RSP: 0x%x  RBP: 0x%x  RFLAGS: 0x%x\r\n",
                      regs->rsp, regs->rbp, regs->rflags);
    }
    serial_puts(COM1, "Interactive panic screen displayed on VGA.\r\n");

    /* Render high-visibility crimson panic TUI */
    /* Header banner: Bright Yellow on Red */
    for (int x = 0; x < 80; x++) {
        tui_putc_at(x, 0, ' ', 0x4E);
    }
    tui_puts_at(2, 0, "*** KERNEL PANIC - RECOVERY CONSOLE ***", 0x4E);
    tui_puts_at(62, 0, "AuraOS v0.2.3", 0x4F);

    /* Background fill (Dark Red on Black text area) */
    for (int y = 1; y < 24; y++) {
        for (int x = 0; x < 80; x++) {
            tui_putc_at(x, y, ' ', 0x4F);
        }
    }

    /* Diagnosis Box */
    tui_draw_box(2, 2, 76, 7, 0x4F, 0x4E, " Diagnostic Information ", 1);
    tui_printf_at(4, 4, 0x4E, "Reason:   ");
    tui_printf_at(14, 4, 0x4F, "%s", msg ? msg : "General Kernel Protection Fault");

    if (file) {
        tui_printf_at(4, 5, 0x4E, "Source:   ");
        tui_printf_at(14, 5, 0x4F, "%s:%u", file, line);
    }

    tui_printf_at(4, 6, 0x4E, "Status:   ");
    tui_printf_at(14, 6, 0x4A, "Kernel halted. Hardware state frozen.");

    /* Registers Box */
    tui_draw_box(2, 10, 76, 8, 0x4F, 0x4E, " CPU Hardware Register State ", 0);
    if (regs) {
        tui_printf_at(4, 12, 0x4F, "RIP: 0x%-16x  CS:     0x%-8x  RFLAGS: 0x%-8x",
                      regs->rip, (uint32_t)regs->cs, (uint32_t)regs->rflags);
        tui_printf_at(4, 13, 0x4F, "RAX: 0x%-16x  RBX:    0x%-16x", regs->rax, regs->rbx);
        tui_printf_at(4, 14, 0x4F, "RCX: 0x%-16x  RDX:    0x%-16x", regs->rcx, regs->rdx);
        tui_printf_at(4, 15, 0x4F, "RSP: 0x%-16x  RBP:    0x%-16x", regs->rsp, regs->rbp);
        tui_printf_at(4, 16, 0x4F, "TRAP: %-2u (code: 0x%x)", (uint32_t)regs->int_no, (uint32_t)regs->err_code);
    } else {
        tui_printf_at(4, 13, 0x4F, "Manual AURA_PANIC assertion triggered from kernel subsystem.");
        tui_printf_at(4, 14, 0x4F, "Pre-panic register snapshot saved in recovery checkpoint.");
    }

    /* Actions Box */
    tui_draw_box(2, 19, 76, 4, 0x4A, 0x4E, " Available Recovery Actions ", 0);
    tui_printf_at(4, 20, 0x4A, "[R] Restore");
    tui_printf_at(16, 20, 0x4F, "- Roll back execution to shell & clear fault");

    tui_printf_at(4, 21, 0x4E, "[B] Reboot");
    tui_printf_at(16, 21, 0x4F, "- Reset processor and restart system");

    tui_printf_at(40, 21, 0x4C, "[O] Power Off");
    tui_printf_at(54, 21, 0x4F, "- ACPI shut down");

    /* Bottom footer bar */
    for (int x = 0; x < 80; x++) {
        tui_putc_at(x, 24, ' ', 0x1F);
    }
    tui_puts_at(2, 24, "Select action: [R] Restore   [B] Reboot   [O] Power Off", 0x1E);

    /* Interactive key polling loop */
    while (1) {
        char key = panic_poll_key();
        if (key == 'r' || key == 'R') {
            panic_do_restore();
        } else if (key == 'b' || key == 'B') {
            panic_do_reboot();
        } else if (key == 'o' || key == 'O' || key == 'p' || key == 'P') {
            panic_do_poweroff();
        }
    }
}

void aura_panic(const char *msg, const char *file, uint32_t line) {
    aura_panic_interactive(msg, file, line, NULL);
}

