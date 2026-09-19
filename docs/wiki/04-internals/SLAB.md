# Slab Zone Allocator (`zalloc` / `zfree`)

**Component:** `[AURA_COMPONENT: SLAB_ZONE_ALLOCATOR]`
**Layer:** Stage 3 — Memory Virtualization
**Contract:** PRD-05-resources-telemetry

---

## Overview

The slab allocator is a fast, constant-time memory allocator for small objects
(16–2048 bytes). It avoids the overhead of the general-purpose free-list heap
(`kmalloc`/`kfree`) by segregating allocations into **8 power-of-2 zones**, each
backed by 4KB pages carved into fixed-size chunks.

```
zalloc(64)   →   zone[2] (64-byte chunks)   →   pop from free list
zfree(ptr)   →   zone[2]                     →   push onto free list
```

---

## Zone Table

| Zone Index | Chunk Size | Chunks per 4KB Page | Max Objects Before Refill |
|:----------:|:----------:|:--------------------:|:--------------------------:|
| 0          | 16 B       | 256                  | 256                        |
| 1          | 32 B       | 128                  | 128                        |
| 2          | 64 B       | 64                   | 64                         |
| 3          | 128 B      | 32                   | 32                         |
| 4          | 256 B      | 16                   | 16                         |
| 5          | 512 B      | 8                    | 8                          |
| 6          | 1024 B     | 4                    | 4                          |
| 7          | 2048 B     | 2                    | 2                          |

Requests larger than 2048 bytes return `NULL` — use `kmalloc()` instead.

---

## Allocation Flow

```
zalloc(size)
    │
    ├─ size == 0 ?           → return NULL
    ├─ size > 2048 ?         → return NULL
    │
    ▼
  Pick smallest zone where chunk_size >= size
    │
    ▼
  zone.free_list == NULL ?
    ├─ YES → pmm_alloc_frame() → carve 4KB into chunks → link free list
    └─ NO  → continue
    │
    ▼
  Pop head of free list → return pointer

zfree(ptr, size)
    │
    ├─ ptr == NULL ?         → no-op
    ├─ size == 0 ?           → no-op
    │
    ▼
  Push chunk onto zone free list (LIFO)
```

---

## Why Power-of-2?

1. **No fragmentation within zones:** Every chunk is exactly `chunk_size` bytes.
   There are no split/merge operations — every free slot is identical.

2. **O(1) allocation and free:** Pop/push a single pointer. No scanning, no
   coalescing, no boundary tags.

3. **Minimal metadata:** The intrusive free list stores the `next` pointer
   *inside* the free chunk itself — zero per-chunk header overhead.

4. **Predictable alignment:** All 16-byte chunks are 16-byte aligned, all
   64-byte chunks are 64-byte aligned, etc. This satisfies most hardware
   DMA alignment requirements up to 2048 bytes.

---

## Source Files

| File | Purpose |
|------|---------|
| `kernel/include/aura/slab.h` | Public API: `slab_init`, `zalloc`, `zfree` |
| `kernel/core/slab.c` | Zone table, free list management, PMM page refill |
| `tests/test_slab.c` | Host unit tests (all sizes, LIFO, exhaustion, edge cases) |
