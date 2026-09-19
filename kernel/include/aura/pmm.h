#ifndef AURA_PMM_H
#define AURA_PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096UL

void      pmm_init(uintptr_t mem_size, uintptr_t bitmap_base);
void      pmm_mark_used(uintptr_t paddr);
void      pmm_mark_free(uintptr_t paddr);
uintptr_t pmm_alloc_frame(void);
void      pmm_free_frame(uintptr_t paddr);
uintptr_t pmm_alloc_contiguous(size_t frame_count);
size_t    pmm_get_free_memory(void);
size_t    pmm_get_used_memory(void);
size_t    pmm_get_total_memory(void);

#endif /* AURA_PMM_H */
