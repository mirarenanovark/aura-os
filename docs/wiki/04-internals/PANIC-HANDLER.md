# Panic Handler

## Overview

`aura_panic()` is the kernel's fatal error handler. It disables interrupts, prints a diagnostic banner to both VGA and serial, then halts the CPU permanently.

## Call Path

```
aura_panic(msg, file, line)         [kernel/core/panic.c]
    │
    ├─ cli                          Disable all maskable interrupts
    │
    ├─ VGA output (0xB8000)
    │   ├─ vga_set_color(WHITE, RED)
    │   ├─ vga_clear()              Fill screen with white-on-red
    │   ├─ vga_puts("=== KERNEL PANIC ===")
    │   ├─ vga_puts(msg)
    │   ├─ vga_puts("at file:line") (if file != NULL)
    │   └─ vga_puts("System halted.")
    │
    ├─ Serial output (COM1 0x3F8)
    │   ├─ serial_puts("\r\n=== KERNEL PANIC ===\r\n")
    │   ├─ serial_puts(msg)
    │   ├─ serial_printf("at file:line") (if file != NULL)
    │   └─ serial_puts("System halted.\r\n")
    │
    └─ for (;;) { hlt; }            Infinite halt loop — CPU stops
```

## VGA White-on-Red

The panic handler sets the VGA text attribute to **white foreground (0xF) on red background (0x4)**:

```
Attribute byte = (RED << 4) | WHITE = 0x4F
```

`vga_clear()` fills all 2000 cells (80×25) with space characters and this attribute, producing a solid red screen. The panic message is then written starting at position (0,0).

## Serial Output

After `serial_init(COM1, 115200)` runs during boot, the COM1 UART at I/O port `0x3F8` is ready. The panic handler uses `serial_puts()` and `serial_printf()` to transmit the same banner over serial, visible in:

- QEMU: `-serial stdio` or `-serial file:serial.log`
- Real hardware: connected terminal or log capture device

No polling for transmitter-ready — serial writes are blocking (spin on LSR bit 5).

## cli; hlt Semantics

```
cli     — Clear Interrupt Flag (RFLAGS.IF = 0)
        — All maskable interrupts (PIC, PIT, keyboard) are masked
        — NMIs and SMI still delivered (not maskable)

hlt     — Halt CPU until next interrupt or reset
        — With IF=0, hlt blocks until NMI or hardware reset
        — CPU enters low-power C1 state
```

The infinite loop `for (;;) { hlt; }` guarantees the CPU never executes any further instructions. A hardware reset or power cycle is required to recover.

## Panic Trigger Points

| Trigger                        | Example                              |
|--------------------------------|--------------------------------------|
| Unhandled CPU exception        | Double fault, page fault, GPF        |
| Assertion failure              | `ASSERT(condition)` macro            |
| Out of memory (non-recoverable)| PMM allocation fails critically      |
| Stack corruption detected      | Canary mismatch                      |
| Subsystem init failure         | IDT or GDT setup fails               |

## Source

`kernel/core/panic.c` — `aura_panic()` in `kernel/include/aura/panic.h`
