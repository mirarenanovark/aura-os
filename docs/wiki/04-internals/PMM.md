# PMM (Physical Memory Manager)

## Overview

The PMM tracks every 4KB physical RAM frame with a bitmap: `1` bit = used, `0` bit = free. `pmm_alloc_frame()` finds a free frame and returns its physical address.

## Where the Bitmap Lives

`pmm_init(mem_size, bitmap_base)` places the bitmap at the physical address passed by `kernel/main.c`.

The bitmap is **dynamically located** immediately after the kernel image:

```c
extern uint8_t _end[];                              /* linker.ld: end of BSS */
uintptr_t bitmap_phys = ((uintptr_t)_end + 4095) & ~4095UL;  /* page-align */
pmm_init(ram_size, bitmap_phys);
```

After initialization, all frames from `0` through the end of the bitmap are marked reserved:

```c
size_t reserved_frames = (bitmap_phys + bitmap_size + 4095) / PAGE_SIZE;
for (size_t f = 0; f < reserved_frames; f++) pmm_mark_used(f * PAGE_SIZE);
```

This prevents the PMM from handing out the kernel's own code, data, BSS, or bitmap memory. One bit per 4KB frame means 128MB of RAM needs only a 4KB bitmap.

## Frame Allocation: 3-Phase Word Scan

`pmm_alloc_frame()` scans the bitmap 64 bits at a time instead of bit-by-bit:

```
pmm_alloc_frame()
    │
    ├─ Fast exit: used == total ? return 0 (out of memory)
    │
    ├─ Phase 1: scan 64-bit words from alloc_hint → end
    │     word value != all-ones  →  a free bit exists
    │     __builtin_ctzll(~word)  →  index of first 0 bit (single CPU instruction)
    │
    ├─ Phase 2: wrap around, scan words 0 → alloc_hint
    │
    ├─ Phase 3: trailing bits (if total_frames is not a multiple of 64)
    │
    └─ Found: set bit, used_frames++, move alloc_hint, return frame address
```

### Why this is fast

- **Word skipping:** a full 64-frame region with no free frame is rejected in one comparison (`word == ~0ULL`). The old code tested 64 bits individually.
- **`__builtin_ctzll`:** compiles to one `tzcnt`/`bsf` CPU instruction — finds the first free bit inside a word instantly.
- **`alloc_hint`:** remembers where the last allocation happened. The next search starts there instead of always scanning from frame 0, so repeated allocations don't re-scan already-used low memory.

### Keeping the hint correct

- `pmm_free_frame()` lowers `alloc_hint` if the freed frame sits below it — otherwise a freed low frame would be invisible until the scan wraps.
- `pmm_init()` resets the hint to 0.

## Contiguous Allocation

`pmm_alloc_contiguous(n)` walks frames bit-by-bit counting consecutive free slots (DMA buffers and heap expansion need physically adjacent frames). It is rare and correctness matters more than speed, so it stays simple.

## Accounting

| Function | Returns |
|:---|:---|
| `pmm_get_free_memory()` | `(total - used) × 4096` bytes |
| `pmm_get_used_memory()` | `used × 4096` bytes |
| `pmm_get_total_memory()` | `total × 4096` bytes |

## Source

- `kernel/core/pmm.c` — bitmap state, 3-phase word scan, contiguous allocator.
- `kernel/include/aura/pmm.h` — public contract (`pmm_init`, `pmm_alloc_frame`, …).
- `tests/test_pmm.c` — host-compiled unit tests (64MB simulated RAM).
