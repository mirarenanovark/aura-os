# Technical Report: Architectural Analysis of iOS / Darwin Memory Management Subsystems for Custom OS Design

## Executive Summary

iOS’s historic ability to outperform competing platforms while using half or a third of the physical RAM (e.g., fluid multitasking on 1GB–2GB RAM) is not accidental. It is the direct consequence of an integrated, vertically co-designed memory architecture spanning **hardware microarchitecture, kernel page management, userspace runtime, and graphics pipelines**.

This report breaks down the exact technical mechanisms implemented in Apple's Darwin / XNU kernel and userspace runtimes to serve as an architectural specification for designing a custom S-tier memory subsystem.

---

## 1. Core Philosophy: The Zero-Waste / Bounded-Footprint Model

Traditional desktop operating systems (Linux, Windows) treat memory as an elastic resource with virtual swap backed by secondary storage. Android adopts a Linux base with JVM/ART garbage collection.

iOS rejects both paradigms:
1. **Zero Secondary Storage Swap Thrashing:** Flash storage endurance and write bandwidth are protected; dirty pages are never lazily paged to block storage.
2. **Determinism over Laziness:** Deferred cleanup (tracing GC) is prohibited in the core UI and system tiers.
3. **Hard Structural Isolation:** Applications are given deterministic memory budgets, strictly enforced at the kernel layer with immediate eviction (`SIGKILL`) rather than degraded frame drops.

---

## 2. Kernel-Level Virtual Memory (XNU VM Subsystem)

### A. The Clean vs. Dirty Memory Model
The kernel divides every physical frame allocated to a task into two primary classes:

* **Clean Memory:**
  - Read-only executable text sections (`__TEXT`).
  - Read-only constant data (`__DATA_CONST`).
  - Memory-mapped files (`mmap` with `MAP_SHARED`/`MAP_PRIVATE` read-only).
  - Framework binary shared regions.
  - *Eviction Policy:* Zero-cost reclamation. Under memory pressure, clean pages are immediately stripped from the page table entries (PTEs) without being written to disk. If re-accessed, the page fault handler re-reads them from block storage on demand.

* **Dirty Memory:**
  - Heap allocations (`malloc`, `kalloc`).
  - Stack allocations.
  - Modified data segments (`__DATA_DIRTY`).
  - Decoded graphics bitmaps / framebuffers.
  - *Eviction Policy:* Cannot be dropped without data loss. Managed strictly via in-RAM compression or process termination.

### B. In-Memory Compressed Pager (`vm_compressor.c`)
Because dirty anonymous memory is never swapped to NAND storage, XNU uses an in-RAM compressor subsystem:

```
[ Active Dirty Page ] ──> (Pressure Event) ──> [ WKdm / LZ4 Compressor ]
                                                      │
                                                      ▼
[ Decompress on Fault ] <── (Page Fault) <── [ Packed In-RAM Compressed Segment ]
```

* **Compression Engines:**
  - **WKdm (Wilson-Kaplan Direct Mapping):** Tuned specifically for in-memory word patterns (pointers, small integers, zero-heavy blocks). Operates at gigabytes-per-second throughput per core.
  - **WK4x4 / LZ4:** Used for multi-page block compaction.
* **Segment Packing:** Compressed pages are packed into dedicated 128KB/256KB slab segments (`c_segment`). This eliminates page fragmentation and yields average compression ratios between **2.5:1 and 3.2:1**.
* **Four Operating Modes:** The compressor shifts modes based on global memory pressure:
  1. `VM_PAGER_DEFAULT` (uncompressed active pages)
  2. `VM_PAGER_COMPRESSOR_NO_SWAP` (all dirty memory compressed in-RAM; standard iOS mode)
  3. `VM_PAGER_COMPRESSOR_WITH_SWAP` (used on macOS when backing store is enabled)
  4. `VM_PAGER_FREEZE` (freezing background tasks into compressed blobs)

### C. Freezing Engine (`kern_memorystatus_freeze.c`)
When an application moves to the background:
1. The kernel signals the task to suspend threads.
2. The `memorystatus` freezer traverses the process's resident dirty pages.
3. The pages are compressed into a unified, frozen memory blob.
4. The physical frames are returned to the free page pool.
5. When the user resumes the app, the pages are lazily or eagerly decompressed back into place, restoring full UI state in milliseconds without a cold application launch.

---

## 3. The `memorystatus` & Jetsam Subsystem (`kern_memorystatus.c`)

Instead of standard Linux OOM killers that evaluate heuristically (`oom_score_adj`), XNU implements **Jetsam**—a deterministic, priority-banded resource governor.

### Priority Bands
Processes are registered into prioritized run-lists:

```
Band 0-2:   Idle / Suspended background apps (First to be killed)
Band 3-5:   Background audio / Background network transfers
Band 6-8:   Mail / Messaging daemon helpers
Band 9-10:  Foreground UI applications
Band 11-15: System Core (SpringBoard/Compositor, launchd, kernel daemons)
```

### High Water Marks & Per-Task Limits
* Every task has two memory limits defined by system profiles:
  - **Active Limit:** Upper bound allowed when in foreground.
  - **Inactive Limit:** Bounded footprint allowed when moved to background (often < 50MB).
* If a process exceeds its Dirty Footprint threshold at any point, the kernel immediately triggers a synchronous Jetsam kill (`EXC_RESOURCE` / `SIGKILL`).
* **Result:** The system never allows a leaking or greedy application to degrade system responsiveness or cause compositor stutter.

---

## 4. Shared Caching & Memory Deduplication: `dyld_shared_cache`

In standard UNIX/Linux environments, dynamic linkers (`ld.so`) load `.so` files into memory, requiring per-process symbol resolution, relocation fixups, and per-process dirty page overhead for writable GOT/PLT entries.

iOS solves this with the **`dyld` Shared Cache**:

```
[ Disk: Single Monolithic Binary (dyld_shared_cache) ]
               │  mmap() once
               ▼
[ Physical Memory: Shared Read-Only Physical Pages ]
        │                       │
        ├───────────────────────┤
        ▼                       ▼
[ Process 1 (App A) ]   [ Process 2 (App B) ] ... [ Process N ]
```

1. **Build-Time / Update-Time Optimization:** All OS libraries (UIKit, Foundation, libSystem, Metal runtime) are merged into one contiguous memory blob.
2. **Pre-Slid Relocations:** All internal symbol cross-references and virtual method tables are resolved ahead of time.
3. **Zero Dirty Relocation Pages:** Because relocations are pre-computed, all library code pages remain **100% clean** and are shared across every single process in the OS via copy-on-write mapping.
4. **Impact:** If 60 processes import the core UI and system frameworks, the memory footprint for those frameworks across the entire system is measured **once** (e.g., ~1.2GB shared physical RAM), adding 0MB per additional spawned task.

---

## 5. Userspace Runtime & Memory Ownership

### Automatic Reference Counting (ARC) vs. Tracing GC

| Metric | Tracing Garbage Collection (JVM / V8 / Go) | Deterministic Reference Counting (ARC / RAII) |
|---|---|---|
| **Memory Overhead** | **200% - 300%** (Requires large free heap headroom to prevent constant GC sweeps) | **~0%** (Only raw object headers + reference counters) |
| **Destruction Timing** | Non-deterministic (Batched during sweeps) | Immediate (Instant frame reclamation upon scope exit) |
| **Latency Consistency** | Prone to Stop-the-World pauses and cache-line invalidations | Predictable, bounded CPU time per deallocation |
| **Cache Locality** | Random heap traversal during mark/sweep phase | High temporal and spatial cache locality |

### Autorelease Pools (`@autoreleasepool`)
To prevent high-frequency temporary object churn from fragmenting the general allocator:
- Autorelease pools use a thread-local stack of memory pages.
- Temporary objects instantiated inside an event loop iteration are registered in the active pool page.
- At the end of the runloop cycle (e.g., before the next 60Hz/120Hz frame render), the pool drains, deallocating hundreds of temporary allocations in a contiguous batch.

### Magazine & Segregated Allocators (`libmalloc`)
iOS's userspace allocator partitions requests by size class to avoid fragmentation:
- **Nano Zone:** Ultra-fast path for allocations $\le 256$ bytes. Uses dedicated arenas mapped with fixed strides, zero metadata overhead per chunk.
- **Tiny Zone:** Allocations up to 1KB (stored in 64KB blocks with inline bitmasks).
- **Small Zone:** Allocations up to 32KB.
- **Large Zone:** Allocations $> 32KB$ allocated directly via `vm_allocate` / `mmap`.
- **Magazine Architecture:** Thread-local allocation caches ("magazines") prevent mutex contention across CPU cores.

---

## 6. Graphics Subsystem: TBDR & Zero-Copy Unified Memory

The graphics and display pipeline is directly responsible for a large percentage of RAM savings on Apple platforms:

### A. Tile-Based Deferred Rendering (TBDR)
Traditional desktop GPUs (Immediate Mode Rendering) shade every primitive drawn, writing intermediate color/depth/stencil data back and forth to main DRAM framebuffers.

Apple Silicon / iOS GPUs operate on a TBDR pipeline:
1. **Tiling Phase:** The frame is divided into small tiles ($16 \times 16$ or $32 \times 32$ pixels).
2. **Hidden Surface Removal (HSR):** The GPU calculates geometry occlusion **before** running pixel/fragment shaders.
3. **On-Chip SRAM Shading:** Color blending, depth testing, and multisampling occur entirely inside ultra-fast, on-chip SRAM registers attached directly to the GPU core.
4. **Single Writeback:** Only the final, fully resolved pixels are written to system RAM once.
5. **Memory Impact:** Eliminates gigabytes-per-second of intermediate scratch framebuffer traffic and multi-megabyte transient frame buffers in RAM.

### B. `IOSurface` & Zero-Copy Compositing
- The display server (Render Server / SpringBoard) and application UI layers share framebuffers via `IOSurface` hardware primitives.
- Decoded video frames, camera frames, and UI render layers are passed between the camera pipeline, video decoder, application layer, and display controller as **shared hardware page descriptors**.
- No copying of image buffers occurs between process boundaries.

---

## 7. OS Implementation Roadmap: Architecture Checklist for an "S-Tier" OS

If you are implementing a custom OS, implement subsystems in this priority order:

```
┌───────────────────────────────────────────────────────────────┐
│                      1. Core Runtime                          │
│   • Choose Rust (RAII / Ownership) or C++ with smart pointers │
│   • Reject tracing garbage collection for system UI and apps  │
└──────────────────────────────┬────────────────────────────────┘
                               │
┌──────────────────────────────▼────────────────────────────────┐
│               2. Pre-Linked Dynamic Shared Cache               │
│   • Merge system dynamic libraries into a single mmap image   │
│   • Pre-calculate all cross-library relocations at link time   │
└──────────────────────────────┬────────────────────────────────┘
                               │
┌──────────────────────────────▼────────────────────────────────┐
│            3. Clean vs. Dirty VM + In-RAM Compressor           │
│   • Tag clean mmap/text pages for zero-cost instant discard   │
│   • Implement WKdm or LZ4 in-kernel page compressor for dirty  │
│   • Never page anonymous dirty memory to flash block storage  │
└──────────────────────────────┬────────────────────────────────┘
                               │
┌──────────────────────────────▼────────────────────────────────┐
│             4. Priority-Banded Killer (Jetsam)                │
│   • Assign strict memory quotas per lifecycle state (Active/  │
│     Background/Idle)                                          │
│   • Enforce hard SIGKILL policy before UI frames drop         │
└──────────────────────────────┬────────────────────────────────┘
                               │
┌──────────────────────────────▼────────────────────────────────┐
│           5. Purgeable Cache Memory Architecture              │
│   • Kernel API to mark cache pages as VOLATILE                │
│   • Kernel reclaims volatile pages silently under pressure    │
└───────────────────────────────────────────────────────────────┘
```

### Essential Reference Code & Papers to Study
1. **XNU Kernel Source:** [github.com/apple-oss-distributions/xnu](https://github.com/apple-oss-distributions/xnu)
   - `osfmk/vm/vm_compressor.c` (In-RAM page compressor)
   - `bsd/kern/kern_memorystatus.c` (Jetsam memory killer)
   - `osfmk/vm/vm_purgable.c` (Purgeable volatile memory)
2. **WKdm Compression Algorithm Paper:** *Wilson, Kaplan, Smaragdakis (1999) — "The Case for Compressed Caching in Virtual Memory Systems"*.
3. **Apple `libmalloc` Source:** [github.com/apple-oss-distributions/libmalloc](https://github.com/apple-oss-distributions/libmalloc) (Nano/Tiny/Small segregated magazine allocator).
