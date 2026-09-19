---
layout: default
title: Roadmap
---

# Engineering Roadmap & Milestone Tracker

**Status:** Phase 1 Boot Foundation is **COMPLETE & VERIFIED** in QEMU.

---

## Phase Completion Diagram (Live Status)

```
                         AuraOS Engineering Journey
                         ==========================

   [PHASE 1: BOOT]          [PHASE 2: MEMORY]        [PHASE 3: SCHEDULER]
   Multiboot2 + GDT + IDT   PMM Bitmap + 4-Lvl VMM   Processes + IPC Rings
        [##########]             [##########]              [###.......]
         100% DONE                90% DONE                  30% DONE
             |                        |                        |
             v                        v                        v
   +---------------------------------------------------------------+
   |  Serial COM1 output verified via QEMU:                        |
   |  [OK] Multiboot2 magic verified                               |
   |  [OK] GDT loaded (kcode=0x08 kdata=0x10)                      |
   |  [OK] IDT loaded (256 gates, PIC remapped 0x20/0x28)          |
   |  [OK] PIT channel 0 configured (divisor 1193)                 |
   |  [OK] PMM ready: 65536 KB free / 65536 KB total               |
   |  [OK] Timer alive: 100 ticks in ~100ms                        |
   +---------------------------------------------------------------+

   [PHASE 4: GLASS UI]      [PHASE 5: APPS]          [PHASE 6: DEV TOOLS]
   Dual Kawase Blur + WM    Sysmon + Terminal        TCC + MicroPython
        [..........]             [..........]             [..........]
         0% PLANNED               0% PLANNED               0% PLANNED
```

---

## Sprint-by-Sprint Breakdown

### Sprint 1: Boot Foundation ✅ **DONE**
- ✅ Multiboot2 header (`0xE85250D6` magic, 8-byte aligned)
- ✅ 32→64-bit long mode trampoline (`boot/entry64.S`)
- ✅ 1GB identity paging via 2MB huge pages
- ✅ 5-entry GDT + 16-byte TSS stub
- ✅ 256-gate IDT + 32 exception stubs + PIC remap (0x20/0x28)
- ✅ 1000Hz PIT heartbeat (divisor 1193)
- ✅ QEMU serial boot verified end-to-end

### Sprint 2: Memory & Preemption 🔄 **IN PROGRESS (90%)**
- ✅ Physical Memory Manager (bitmap allocator, 2KB overhead per 64MB)
- ✅ Virtual Memory Manager (4-level paging: PML4 → PDPT → PD → PT)
- ✅ Host unit tests passing (`make test`)
- ⬜ Kernel heap (kmalloc/kfree slab allocator on top of VMM)
- ⬜ Page-fault handler (demand paging, COW)
- ⬜ i686 2-level paging mirror (`kernel/arch/i686/vmm.c`)

### Sprint 3: Processes & IPC ⏳ **NEXT**
- ⬜ Context switching (TCB structure, Ring 3 entry via `iretq`)
- ⬜ Round-Robin scheduler (PIT-driven, `SCHED_FIFO` support for audio)
- ⬜ Lock-free SPSC shared-memory IPC rings (AUD-001 compatible)
- ⬜ `sys_telemetry` flat struct observatory (TEL-001)

### Sprint 4: RAMFS & Shell
- ⬜ In-memory filesystem (tar/ustar initrd loading via Multiboot2)
- ⬜ Interactive shell over COM1 with line editing
- ⬜ `/proc`-style telemetry virtual filesystem

### Sprint 5: Framebuffer & Glass UI
- ⬜ Linear framebuffer scanout (UEFI GOP + VBE 3.0 dual path)
- ⬜ SIMD (SSE2/NEON) Dual Kawase 4x pyramid blur engine
- ⬜ 9-slice Aero/Frost window frame renderer
- ⬜ `aura-wm` compositor prototype with dirty-rect bounding

### Sprint 6: Native Apps & Dev Environment
- ⬜ `sysmon` task manager (4-tab, wave graphs, <1.2MB RAM)
- ⬜ Terminal app with scrollback
- ⬜ TCC compiler + MicroPython REPL capsules
- ⬜ AuraAudio daemon (SPSC rings, <2.6ms DAW latency)
