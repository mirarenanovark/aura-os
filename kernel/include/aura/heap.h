/**
 * @file        kernel/include/aura/heap.h
 * @layer       STAGE_3_VIRTUAL_SCHEDULER
 * @component   KERNEL_HEAP_FREELIST
 * @contract    PRD-05-resources-telemetry
 * @description Dynamic kernel heap memory allocator interface.
 *              Provides kmalloc, kmalloc_aligned, kfree, krealloc, and usage queries.
 *
 * @connects
 *              - Upstream:   kernel/main.c, kernel drivers and subsystems
 *              - Downstream: kernel/core/heap.c, kernel/core/pmm.c
 */

#ifndef AURA_HEAP_H
#define AURA_HEAP_H

#include <stdint.h>
#include <stddef.h>
#include <aura/pmm.h>

/*
 * Heap allocator – simple free-list on top of PMM-backed pages.
 *
 * On real hardware, physical addresses returned by PMM are identity-mapped
 * (or translated through a fixed offset).  The heap needs to convert
 * physical addresses from PMM into usable virtual pointers.  Set the
 * translation base once before first use via heap_init.
 *
 *   heap_init(0)          – identity map (phys == virt, real kernel)
 *   heap_init(BASE)       – all PMM addresses are offset by BASE
 */

/* Initialize the heap. `phys_offset` is added to every PMM address
 * to produce a usable virtual pointer.  Pass 0 for identity mapping. */
void heap_init(uintptr_t phys_offset);

/* Allocate `size` bytes. Returns NULL on failure. */
void *kmalloc(size_t size);

/* Allocate `size` bytes aligned to `align` (must be power of 2). */
void *kmalloc_aligned(size_t size, size_t align);

/* Free a previous kmalloc/kmalloc_aligned allocation. */
void kfree(void *ptr);

/* Resize an allocation. Returns NULL on failure (original still valid). */
void *krealloc(void *ptr, size_t new_size);

/* Query: total bytes currently allocated (user payload only). */
size_t heap_get_used(void);

/* Query: total bytes available (sum of free block payloads). */
size_t heap_get_free(void);

#endif /* AURA_HEAP_H */
