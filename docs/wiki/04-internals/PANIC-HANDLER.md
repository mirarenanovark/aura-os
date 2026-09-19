---
layout: default
title: Panic Handler
---

# Panic Handler

When the kernel hits an unrecoverable condition, `aura_panic()` takes over the machine and halts it safely.

---

## Call Path

```
AURA_PANIC("message")
  │
  │  Expands to:  aura_panic("message", __FILE__, __LINE__)
  │
  v
aura_panic(msg, file, line)    kernel/core/panic.c
  │
  ├─ cli                       Disable CPU interrupts (NMI-proof halt)
  │
  ├─ VGA output                White on red, full-screen clear
  │   ├─ "=== KERNEL PANIC ==="
  │   ├─ msg                   Caller's error string
  │   ├─ "  at file:line"      Source location (if file != NULL)
  │   └─ "System halted."
  │
  ├─ Serial output             Same text to COM1 (115200 8N1)
  │   ├─ "\r\n=== KERNEL PANIC ===\r\n"
  │   ├─ msg
  │   ├─ "  at file:line"
  │   └─ "System halted.\r\n"
  │
  └─ for (;;) { hlt; }        Halt loop — CPU sleeps until NMI
```

---

## VGA White-on-Red Display

`aura_panic()` owns the full screen. It calls `vga_clear()` to blank all 80×25 text cells, then sets the color attribute to:

- **Foreground:** White (`VGA_COLOR_WHITE` = 0x0F)
- **Background:** Red (`VGA_COLOR_RED` = 0x04)

Every character cell is white-on-red. The banner is:

```
╔══════════════════════════╗
║  === KERNEL PANIC ===    ║
║                          ║
║  <error message>         ║
║  at <file>:<line>        ║
║                          ║
║  System halted.           ║
╚══════════════════════════╝
```

The color is applied via `vga_set_color()` which writes to VGA attribute controller registers (`0x3C0`/`0x3C1`).

---

## Serial Output

The same panic message is written to COM1 (`serial_puts`). This is critical for:

- **QEMU debugging** — output appears on `-serial stdio` even when the VGA display is not visible.
- **Headless servers** — panic messages reach a serial console log.
- **Automated testing** — CI pipelines can grep serial output for `KERNEL PANIC`.

Serial uses `\r\n` line endings (required by most terminal emulators).

---

## `cli; hlt` Semantics

The halt sequence is intentional and structured:

```c
/* 1. cli — Clear Interrupt Flag */
/*    - Disables all maskable interrupts (PIC, PIT, keyboard) */
/*    - CPU will not service IRQs or CPU exceptions 0x00-0x1F */
/*    - NMI (Non-Maskable Interrupt, vector 2) still fires */

/* 2. for (;;) { hlt; } */
/*    - HLT puts the CPU in a low-power wait state */
/*    - Wakes on NMI or SMI (System Management Interrupt) */
/*    - Since interrupts are disabled, normal wake = impossible */
/*    - The loop re-executes HLT after any NMI */
```

This is the only safe halt pattern:

| Pattern | Problem |
|---------|---------|
| `cli` alone | CPU busy-spins, wastes power, may trigger watchdog |
| `hlt` alone (no `cli`) | PIT IRQ0 wakes CPU every 1ms, kernel re-enters interrupt handler |
| `cli; hlt` (no loop) | NMI wakes CPU, code falls through to whatever is after |

The `for (;;)` loop guards against NMI recovery. After an NMI (e.g., hardware watchdog, memory parity error), the CPU re-executes `hlt` and goes back to sleep.

---

## The `AURA_PANIC` Macro

Defined in `kernel/include/aura/panic.h`:

```c
#define AURA_PANIC(msg) aura_panic((msg), __FILE__, __LINE__)
```

The macro captures `__FILE__` and `__LINE__` at the call site. This is a compile-time constant, so:

- No runtime overhead for location capture.
- The file string lives in `.rodata` (read-only memory, not stack).
- Callers never forget to pass location info.

---

## Panic Callers

Panic is used for conditions where the kernel cannot continue safely. Typical triggers:

- **PMM corruption** — bitmap sanity check failed, double-free detected
- **VMM violation** — page table walk reached an invalid state
- **Stack overflow** — guard page hit (future)
- **Assertion failures** — invariant broken, kernel state inconsistent

Panic is **not** used for user-space errors (those get syscall error codes), recoverable faults (page faults map new pages), or expected conditions (no free frames → wait/kill).

---

## Design Constraints

The panic handler must work in the worst possible conditions:

| Constraint | Design Choice |
|------------|---------------|
| No dynamic allocation | `vga_puts()` writes directly to VGA memory, no heap needed |
| No `printf` dependency | Line number uses inline `itoa` (base-10 loop, no libc) |
| No interrupt reliance | `cli` disables before any I/O — panic works even if IDT is corrupt |
| Dual output path | VGA + serial — at least one will be visible in any environment |
| Zero stack depth | Shallow call chain — panic works even on a corrupted stack |

---

## Source Files

| File | Role |
|------|------|
| `kernel/include/aura/panic.h` | `aura_panic()` declaration, `AURA_PANIC` macro |
| `kernel/core/panic.c` | Implementation: VGA clear, serial write, `cli; hlt` loop |
| `kernel/include/aura/vga.h` | `vga_set_color()`, `vga_clear()`, `vga_puts()` |
| `kernel/include/aura/serial.h` | `serial_puts()`, `serial_putc()`, `serial_printf()` |
