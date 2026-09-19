/**
 * @file        kernel/include/aura/slab.h
 * @layer       STAGE_3_VIRTUAL_SCHEDULER
 * @component   SLAB_ZONE_ALLOCATOR
 * @contract    PRD-05-resources-telemetry
 * @description Power-of-2 segregated slab zone allocator interface.
 *              Provides zalloc/zfree for fixed-size buckets 16..2048 bytes.
 *
 * @connects
 *              - Upstream:   kernel subsystems requiring small fast allocations
 *              - Downstream: kernel/core/slab.c, kernel/core/pmm.c
 */

#ifndef AURA_SLAB_H
#define AURA_SLAB_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the slab allocator zones.
 */
void slab_init(void);

/**
 * @brief Allocate a chunk from the smallest slab zone that fits `size`.
 * @param size Requested size in bytes (1..2048).
 * @return Pointer to chunk, or NULL if size is 0 or > 2048.
 */
void *zalloc(size_t size);

/**
 * @brief Return a previously zalloc'd chunk to its zone free list.
 * @param ptr  Pointer returned by zalloc.
 * @param size Size originally passed to zalloc.
 */
void zfree(void *ptr, size_t size);

/**
 * @brief Set the physical-to-virtual translation offset (used by host tests).
 */
void slab_set_phys_offset(uintptr_t offset);

#endif /* AURA_SLAB_H */
