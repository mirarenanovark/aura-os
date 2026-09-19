/**
 * @file        kernel/core/pmm.c
 * @layer       STAGE_2_PHYSICAL_FAULT
 * @component   PMM_BITMAP_ALLOCATOR
 * @contract    PRD-05-resources-telemetry
 * @description 4KB frame physical memory manager using a bit-level bitmap.
 *              Each bit tracks 1 physical frame (1 = allocated, 0 = free).
 *              Provides single frame allocation and contiguous frame allocation for
 *              heap expansions and DMA buffers.
 *
 * @connects
 *              - Upstream:   kernel/main.c:kernel_main (pmm_init), kernel/core/heap.c (grow_heap),
 *                            kernel/arch/x86_64/vmm.c (page table levels)
 *              - Hardware:   Raw physical address space
 *
 * @flow        [AURA_FLOW: PMM_FRAME_MGMT]
 *              1. pmm_init(): Clears bitmap across total_frames based on detected RAM size.
 *              2. pmm_alloc_frame(): Scans bitmap for first '0' bit, sets to '1', returns paddr.
 *              3. pmm_free_frame(): Clears bit in bitmap, decrements used_frames counter.
 *              4. pmm_alloc_contiguous(): Finds N adjacent free bits, marks all used.
 */

#include <aura/pmm.h>

/* Internal State */
static uint8_t *bitmap = (void*)0;
static size_t total_frames = 0;
static size_t used_frames = 0;

/* Helper Macros */
#define BITMAP_SET(bit)   (bitmap[(bit) / 8] |= (uint8_t)(1 << ((bit) % 8)))
#define BITMAP_CLEAR(bit) (bitmap[(bit) / 8] &= (uint8_t)~(1 << ((bit) % 8)))
#define BITMAP_TEST(bit)  ((bitmap[(bit) / 8] & (uint8_t)(1 << ((bit) % 8))) != 0)

void pmm_init(uintptr_t mem_size, uintptr_t bitmap_base) {
    bitmap = (uint8_t *)bitmap_base;
    total_frames = mem_size / PAGE_SIZE;
    used_frames = 0;

    /* Initialize all memory as free (0) */
    size_t bitmap_size = (total_frames + 7) / 8;
    for (size_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0;
    }
}

void pmm_mark_used(uintptr_t paddr) {
    size_t frame = paddr / PAGE_SIZE;
    if (frame < total_frames && !BITMAP_TEST(frame)) {
        BITMAP_SET(frame);
        used_frames++;
    }
}

void pmm_mark_free(uintptr_t paddr) {
    size_t frame = paddr / PAGE_SIZE;
    if (frame < total_frames && BITMAP_TEST(frame)) {
        BITMAP_CLEAR(frame);
        used_frames--;
    }
}

uintptr_t pmm_alloc_frame(void) {
    for (size_t i = 0; i < total_frames; i++) {
        if (!BITMAP_TEST(i)) {
            BITMAP_SET(i);
            used_frames++;
            return (uintptr_t)(i * PAGE_SIZE);
        }
    }
    return 0; /* Out of memory */
}

void pmm_free_frame(uintptr_t paddr) {
    pmm_mark_free(paddr);
}

uintptr_t pmm_alloc_contiguous(size_t frame_count) {
    if (frame_count == 0) return 0;

    size_t count = 0;
    size_t start = 0;

    for (size_t i = 0; i < total_frames; i++) {
        if (!BITMAP_TEST(i)) {
            if (count == 0) start = i;
            count++;
            if (count == frame_count) {
                /* Mark all frames in range as used */
                for (size_t j = start; j < start + frame_count; j++) {
                    BITMAP_SET(j);
                }
                used_frames += frame_count;
                return (uintptr_t)(start * PAGE_SIZE);
            }
        } else {
            count = 0;
        }
    }
    return 0; /* Contiguous block not found */
}

size_t pmm_get_free_memory(void) {
    return (total_frames - used_frames) * PAGE_SIZE;
}

size_t pmm_get_used_memory(void) {
    return used_frames * PAGE_SIZE;
}

size_t pmm_get_total_memory(void) {
    return total_frames * PAGE_SIZE;
}
