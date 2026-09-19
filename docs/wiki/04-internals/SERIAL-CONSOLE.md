# Serial Console (UART 16550 COM1)

## Overview

The serial console provides headless input/output via the 16550 UART at COM1 (port 0x3F8). It enables remote shell access when no VGA/keyboard is connected — essential for embedded, server, or QEMU headless operation.

## UART 16550 LSR Register Bit 0

The Line Status Register (LSR) at offset `COM1 + 5` (0x3FD) reports transmitter/receiver status. Bit 0 is **Data Ready (DR)**:

| Bit | Name | Meaning |
|-----|------|---------|
| 0   | DR   | 1 = byte received and ready to read from Data Register (0x3F8) |
| 5   | THRE | 1 = transmitter holding register empty (ready to write) |

Polling LSR bit 0 is the simplest non-blocking receive method — no IRQs, no DMA, no buffering.

## ASCII Call Path

```
keyboard polling (PS/2) ──┐
                          ├─► menu_handle_key(c) ──► execute_command()
serial polling (COM1)  ───┘        ▲
      │                            │
      │  serial_has_char(COM1)     │
      │  serial_getc(COM1)         │
      ▼                            │
  UART 16550 LSR bit 0 (DR) ──────┘
```

## Driving the Shell Over Headless Serial (QEMU)

**QEMU command:**
```bash
qemu-system-x86_64 -serial mon:stdio -nographic ...
```

**What this does:**
- `-serial mon:stdio` connects guest COM1 to the host terminal (merged with QEMU monitor via `mon:`).
- `-nographic` disables VGA; all output goes to serial.
- Type commands at the `aura>` prompt — they arrive via serial RX polling.
- Press **Enter** (sends `\r`) — the kernel translates it to `\n` for command execution.

**Without `-nographic`** (QEMU window + serial):
```bash
qemu-system-x86_64 -serial stdio ...
```
Opens a separate terminal window for serial I/O alongside the VGA display.

## Source

| File | Purpose |
|------|---------|
| `kernel/arch/x86_shared/serial.c` | UART driver: TX/RX polled I/O, `serial_has_char()`, `serial_getc()` |
| `kernel/include/aura/serial.h` | Public API declarations |
| `kernel/main.c` | Idle loop polls `serial_has_char(COM1)` and feeds `menu_handle_key()` |
