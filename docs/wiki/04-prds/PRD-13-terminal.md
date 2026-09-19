The user wants a rigorous PRD for AuraOS Component #13: Terminal Streams, Escape Parser & Console Runtime. This is a freestanding C OS project targeting 4 architectures. I need to write production-grade markdown with the exact 6-section structure.

Key considerations:
- AuraOS is freestanding C — no libc, no dynamic allocation unless via a kernel allocator component (I should reference plausibly-named sibling components, e.g., Component #N naming). Since the prompt says "Component #13," I can reference other components by number cautiously — better to reference by name generically (e.g., "Kernel Heap (Component #7)" — but inventing wrong numbers is risky). I'll reference components by name with placeholder-free naming, e.g., `aura_heap`, `aura_spinlock`, etc. Actually a PRD for a component in a series typically references sibling components. I'll use names like "KMem", "KSync", "Early Console Drivers" — but keep it generic enough. To be safe, I'll reference "the kernel heap allocator" and "framebuffer console driver" without fake numbers, or use plausible component numbers sparingly. I'll reference "Component #12 (Device Framework)" and "Component #14 (Keyboard Input)" — that's inventing. Better: reference by module name only: `term`, `con`, `esc`. That's cleanest and avoids fabricating the numbering of the rest of the system.

4 arches: x86_64, i686, aarch64, riscv64. The terminal layer is arch-agnostic; console backend drivers are arch/board-specific. Must specify that.

The component covers:
1. Terminal streams (master/slave-ish? No — this is likely kernel console + virtual terminals). Keep it small: ring-buffer byte streams feeding a VT parser, multiple virtual consoles (VTs), output to a console backend (text-mode VGA, serial, framebuffer).
2. Escape parser: a strict subset of ANSI/VT (CSI, OSC maybe minimal, SGR, cursor movement, erase, scroll region). A tiny DFA parser — like a Paul Williams VT500 parser state machine, simplified. Must define exactly which sequences are supported and what is ignored.
3. Console runtime: scheduling of flushes, cursor management, scrollback, virtual terminal switching, multiplexing to multiple sinks (serial + framebuffer).

Grounding principles: Clean. Simple. Small. Fast. Direct. No overengineering. So:
- Fixed-size static or pool-allocated structures. No dynamic allocation in the hot path; one-time init allocation.
- Bounded everything: VT count, scrollback lines, stream buffer sizes.
- No UTF-8 full-width/grapheme handling — maybe require UTF-8 pass-through but treat code points as cells only if in BMP and width 1? Keep simple: ASCII fast path + UTF-8 decode to codepoint, width via tiny table for common ranges? "No overengineering" suggests: 8-bit clean byte transport; console renders bytes; bytes >= 0x80 are passed to backend as-is; combining chars NOT handled. State clearly in anti-goals.

Let me define the wire ABI contracts:
- `/dev/console` semantics? This is a freestanding kernel component — the "wire ABI" is likely: syscall interface (read/write/ioctl on console fd) + internal kernel interfaces. Define ioctls: TCGETS? That's termios — heavy. Keep a minimal ioctl set: get/set active VT, bell, etc. Actually "Wire ABI Contracts" — for this component, the ABI contracts are: (a) kernel-internal C API, (b) userland syscall ABI for console streams, (c) the byte-level escape sequence grammar accepted (the "wire" being the byte stream). I'll define all three: the escape grammar as a formal contract, the ioctl numbers as stable ABI.

Let me draft the structure:

# PRD: AuraOS Component #13 — Terminal Streams, Escape Parser & Console Runtime

Metadata block: ID, status, owner, dependencies, target arches, LOC budget. LOC budget is a great "no overengineering" enforcement: e.g., ≤1,500 SLOC core, ≤2,500 total including drivers? Give hard ceilings.

## 1. Executive Summary & Scope Boundary
- What it is: kernel-resident byte-stream terminal multiplexer with a bounded, table-free (or minimal-table) ANSI escape parser, N virtual consoles, output fan-out to registered console sinks.
- In scope: ring buffer streams, VT parser (CSI/SGR/OSC-minimal), cell grid state, scroll region, cursor, VT switch, blink-less block cursor, backend interface (text & fb), kernel-side `kprintf` routing.
- Out of scope (pointer to anti-goals section): pty/termios job control, UTF-8 shaping, mouse, alternate screen? Alternate screen is cheap-ish but "no overengineering" — exclude. Full DEC private modes — subset only.

## 2. Core Requirements
Functional:
- FR-1: N virtual terminals (default 4, compile-time max 8), each with cell grid + attribute state.
- FR-2: Byte-stream ingestion API for kernel (`con_write`) and userland via console device.
- FR-3: Deterministic escape parser: exact state machine, fixed ≤4 states? Let me count: GROUND, ESC, CSI, OSC (string), ESC_INTERMEDIATE, CSI_IGNORE. Keep: GROUND, ESC, CSI, OSC, CHARSET? Keep small: GROUND, ESC, CSI intermediate/param, OSC string, IGNORE. I'll spec 6 states max.
- FR-4: Supported control chars: BEL, BS, HT (tab stops every 8, fixed), LF/VT/FF → linefeed (+ optional ONLCR mapping), CR, SO/SI ignored? Spec them.
- FR-5: SGR subset: 0,1,7,22,27,30–37,39,40–47,49,90–97,100–107. No 256-color, no RGB (anti-goal? or in scope?). Keep: 16-color + bold-asa-bright. 256-color/RGB = explicitly ignored (consume sequence, no effect) or excluded? Parsing must still consume them correctly (38;5;n;...). Consume-and-discard is required for safety. That's a good rigorous point: sequences we don't implement must still be consumed deterministically, never leak to screen.
- FR-6: Cursor: CUU/CUD/CUF/CUB/CUP/HVP, CHA, VPA, f.b absolute limits.
- FR-7: Erase: ED (0,1,2,3), EL (0,1,2).
- FR-8: Scroll: SU, SD, DECSTBM (scroll region), IL/DL? IL/DL maybe exclude to stay small. Decide: include IL/DL? They're ~20 lines each. I'll exclude IL/DL, ECH? Include ECH (erase char) — cheap. Keep list tight.
- FR-9: Modes: DECTCEM (cursor hide/show), LNM? Keep DECTCEM only + autowrap DECAWM maybe. Autowrap is basically required for correct behavior; include with default on.
- FR-10: OSC: only OSC 0/2 (title) — stored in fixed 64-byte buffer, consumed; everything else consumed and discarded. Or exclude OSC entirely except "consume safely." Hmm — storing title is bloat-ish but it's tiny and useful. Include OSC 0/2 capture with truncation, everything else discard. Actually for a kernel console, title does nothing (no window). Cleaner: OSC is consumed and discarded entirely. Yes — "Direct." Kernel console has no window title. Put OSC title in anti-goals. But then OSC state exists only to consume. Good.
- FR-11: Device status: none. DSR excluded → consumed.
- FR-12: Fan-out sinks: serial + framebuffer/text; per-VT enable mask.
- FR-13: VT switch: ioctl/F-key binding handled by input component; this component exposes switch API.
- FR-14: Scrollback: ring of lines, default 256 lines per VT? Scrollback is framebuffer console nicety; serial has none. Keep optional at compile time, default on, bounded 512 lines.
- FR-15: Kernel logging path: `klog_write` bypasses parser? Or goes through parser on dedicated VT? Simpler: kernel console output goes through same stream, on VT0 reserved for kernel log. Decide: VT0 = kernel log console, user VTs 1..N-1. That's clean.
- FR-16: Bell: sink callback, no sound by default.

Non-functional:
- NFR-1: Determinism: no unbounded loops — every byte processed in O(1) amortized; worst-case per byte bounded (a scroll = O(width) memcpy, amortized).
- NFR-2: Interrupt context safety: write path callable from IRQ with preemption disabled? At least lock-free single-producer? Keep: spinlock per stream; IRQ-safe.
- NFR-3: Zero allocation after init.
- NFR-4: Memory budget: hard caps table.
- NFR-5: Code size ceilings.
- NFR-6: Performance targets: ≥ 20 MB/s parse throughput on target floor (e.g., 100 MHz class)? Set modest: parse+render ≥ 10 MB/s per VT on reference QEMU arch, no per-byte function pointer dispatch.
- NFR-7: No undefined behavior: all array indexing bounded; fuzz-clean (see acceptance).
- NFR-8: Portability: only C11 freestanding, no arch-specific code in core; drivers arch-specific behind interface.
- NFR-9: Reentrancy rules documented.

## 3. Data Structures, C Interfaces & Wire ABI Contracts
C code blocks:

```c
/* aura/con.h — public kernel interface */
#define CON_MAX_VTS      4
#define CON_MAX_SINKS    3
#define CON_COLS         80
#define CON_ROWS         25
...
typedef struct con_cell { u16 ch; u8 fg:4, bg:4; u8 attr; } con_cell_t; // or bitfields via masks
```

Better to avoid bitfield portability issues? Bitfields of u8 are fine-ish but PR rigor: use explicit masks on u32? Cell = u32: glyph in low 16? Let's do: `typedef struct { u16 ch; u8 attr; } con_cell_t;` where attr packs fg(4)|bg(4). Attr bits: bold as fg bright bit. That's 4 bytes per cell, 80*25*4 = 8000 bytes per VT. 4 VTs = 32KB. Fine. Scrollback 512 lines * 80 * 4 = 160KB per VT? That's 640KB for 4 VTs — too much. Reduce scrollback to 128 lines shared pool? Scrollback pool: 512 lines total shared, per-VT quota? Simplest: scrollback optional, 256 lines per VT of 80 cells = 256*80*4 = 80KB/VT. Hmm. For a lean OS, scrollback per VT 128 lines → 40KB/VT, 4 VTs = 160KB. Acceptable but let me set default scrollback 0 on serial-only builds, 128 with fb. I'll present a memory budget table and make scrollback compile-time.

Cell layout: I'll define u32: bits 0–15 codepoint, 16–19 fg palette index, 20–23 bg, 24–? attr flags (bold, inverse). Actually inverse can be rendered by swapping at draw time; store fg/bg