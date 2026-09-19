# AuraOS Changelog

All releases, newest first. Module scopes: `boot` `kernel` `drivers` `gui` `libs` `userland` `tools` `tests`. See the [Versioning Protocol](wiki/01-governance/versioning.html) for the rules.

## v0.2.2-zram (2026-09-19) — stage: zRAM In-Memory Compression
Milestone update: Lightweight Apple vm_compressor-style in-memory page compressor and pool allocator.
### kernel
- In-memory zRAM page compressor (`kernel/core/zram.c`, `kernel/include/aura/zram.h`) with run-length and zero-word packing
- Freestanding copy semantics preventing SIMD/SSE `movdqa` invalid opcode traps on uninitialized vector registers
- System monitor integration exposing compressed memory ratio and page counts
### tests
- Complete host verification suite for zero page compression, pattern page runs, and store/load/free cycles (`tests/test_zram.c`)
### docs
- Engineering roadmap and milestone tracker (`docs/roadmap.md`, `docs/TASK_QUEUE.md`, `docs/BUGS.md`)

## v0.2.1-tui (2026-09-19) — stage: TUI contrast + live refresh
PATCH release: fixes washed-out dashboard colors and the frozen (render-once) display.
### kernel
- Hi-contrast palette: full-bright foregrounds (green/cyan/yellow/white) on black, no mid-grey text; header title yellow-on-blue
- Dashboard now redraws every 200ms in the kernel idle loop — uptime, ticks, and CPU load update live instead of freezing after boot
### drivers
- tui_draw_box takes separate border + title colors; bars render over a dim `░` track instead of dark-grey `█`

## v0.2.0-core (2026-09-19) — stage: Core Infrastructure & TUI Dashboard
Milestone upgrade: Full memory stack (PMM + doubly-linked heap), crash handler, CPU telemetry, VGA text console, and btop visual dashboard.
### kernel
- Physical Memory Manager (PMM): bitmap frame allocator with contiguous allocations (`kernel/core/pmm.c`)
- Kernel Heap Allocator (`kmalloc`/`kfree`/`krealloc`/`kmalloc_aligned`): doubly-linked free-list with O(1) coalescing (`kernel/core/heap.c`)
- Kernel Panic Handler: `AURA_PANIC()` macro with white-on-red visual halt and serial diagnostic logging (`kernel/core/panic.c`)
- Sysmon Telemetry Engine: tick rate sampling, CPU load percentage, RAM usage metrics (`kernel/core/sysmon.c`)
### drivers
- VGA 80x25 Text Console: direct hardware buffer (`0xB8000`) driver with scrolling, ANSI-style colors, and character cursor (`kernel/drivers/vga.c`)
- TUI Box & Dashboard Engine: CP437 single/double box drawing primitives, gradient bars (`█▓▒░`), and btop-style process telemetries (`kernel/drivers/tui.c`, `kernel/core/dashboard.c`)
### tests
- PMM unit tests covering basic allocation, contiguous blocks, and free-list integrity (`tests/test_pmm.c`)
- Comprehensive 10-suite heap verification test covering edge cases, 100-iteration stress, and alignment (`tests/test_heap.c`)
### docs
- Stage 1 Boot Flow call graph documentation (`docs/wiki/04-internals/BOOT-FLOW.md`)
- Panic handler crash path architecture (`docs/wiki/04-internals/PANIC-HANDLER.md`)

## v0.1.0-uart (2026-09-19) — stage: UART bring-up
First versioned stage. Everything before this point = architecture & docs groundwork.
### kernel
- freestanding 16550 UART driver + serial_printf, clean x86_64/i686 dual compile (bcb376f)
### drivers
- DRV-001/002/003 GPU driver architecture specs, hardware support matrix, master graphics stack spec w/ 4-tier never-black-screen fallback
### docs
- MESA-001 userspace 3D strategy, roadmap milestone cards, docs site (glass navbar, dark frosted), PHILOSOPHY.md, Astra design audits
