/**
 * @file        kernel/core/slab.c
 * @layer       STAGE_3_VIRTUAL_SCHEDULER
 * @component   SLAB_ZONE_ALLOCATOR
 * @contract    PRD-05-resources-telemetry
 * @description Power-of-2 segregated slab zone allocator.
 *              Eight zones cover 16, 32, 64, 128, 256, 512, 1024, 2048 bytes.
 *              Each zone allocates 4KB pages from PMM and carves them into
 *              fixed-size chunks linked by an intrusive singly-linked free list.
 *
 * @connects
 *              - Upstream:   kernel subsystems calling zalloc/zfree
 *              - Downstream: kernel/core/pmm.c (pmm_alloc_frame)
 *
 * @flow        [AURA_FLOW: SLAB_ALLOC]
 *              1. zalloc(size): Pick smallest zone with chunk_size >= size.
 *              2. If zone free list is empty, allocate a 4KB page from PMM.
 *              3. Carve page into chunk_size pieces, link into free list.
 *              4. Pop head of free list and return pointer.
 *              5. zfree(ptr, size): Push chunk back onto zone free list (LIFO).
 */

#include <aura/slab.h>
#include <aura/pmm.h>
#include <stdint.h>

/* ---------- Intrusive free-list chunk ---------------------------------- */

struct slab_chunk {
    struct slab_chunk *next;
};

/* ---------- Zone bookkeeping ------------------------------------------- */

#define SLAB_NUM_ZONES   8
#define SLAB_PAGE_SIZE   4096UL

/* Zone table: power-of-2 chunk sizes. Index 0 = 16B, index 7 = 2048B. */
static const size_t zone_chunk_size[SLAB_NUM_ZONES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

struct slab_zone {
    struct slab_chunk *free_list;   /* Head of intrusive free list */
    size_t             chunk_size;  /* Fixed chunk size for this zone */
    size_t             alloc_count; /* Number of pages allocated */
};

static struct slab_zone zones[SLAB_NUM_ZONES];

/* ---------- phys-to-virt translation ----------------------------------- */

static uintptr_t slab_phys_offset = 0;

static inline void *phys_to_virt(uintptr_t pa) {
    return (void *)(pa + slab_phys_offset);
}

/* ---------- Internal: refill zone from a new PMM page ------------------ */

static void zone_refill(struct slab_zone *z) {
    uintptr_t pa = pmm_alloc_frame();
    if (!pa) return; /* Out of physical memory */

    uint8_t *base = (uint8_t *)phys_to_virt(pa);
    size_t chunks_per_page = SLAB_PAGE_SIZE / z->chunk_size;

    /* Carve page into chunks and push onto free list (reverse order so
       first allocated chunk has lowest address — cosmetic but predictable). */
    for (size_t i = 0; i < chunks_per_page; i++) {
        struct slab_chunk *c = (struct slab_chunk *)(base + i * z->chunk_size);
        c->next = z->free_list;
        z->free_list = c;
    }
    z->alloc_count++;
}

/* ---------- Public API ------------------------------------------------- */

void slab_init(void) {
    /* In the real kernel, phys_offset is 0 (identity map).
       In host tests, the caller must set pmm_init with simulated_ram base
       and slab uses the same offset the heap uses. We derive it from a
       known PMM convention: PMM returns frame index * PAGE_SIZE, and the
       host test passes (uintptr_t)simulated_ram as phys_offset to heap_init.
       For slab, we store the same offset so phys_to_virt works.

       Since slab_init is called after pmm_init and heap_init in both
       kernel_main and tests, we need the phys_offset to be set externally.
       We solve this by making slab_init accept no args and computing the
       offset lazily: we store the offset in a static that tests set via
       slab_set_phys_offset(), or default to 0 in the kernel.

       Simpler: just default to 0 (identity). Host tests will set
       slab_phys_offset before calling slab_init. See tests/test_slab.c.
    */
    for (int i = 0; i < SLAB_NUM_ZONES; i++) {
        zones[i].free_list  = NULL;
        zones[i].chunk_size = zone_chunk_size[i];
        zones[i].alloc_count = 0;
    }
}

/* Allow host tests to set the phys-to-virt offset. */
void slab_set_phys_offset(uintptr_t offset) {
    slab_phys_offset = offset;
}

/* Map requested size to zone index. Returns -1 if no zone fits. */
static int size_to_zone_index(size_t size) {
    for (int i = 0; i < SLAB_NUM_ZONES; i++) {
        if (size <= zone_chunk_size[i]) {
            return i;
        }
    }
    return -1; /* size > 2048 */
}

void *zalloc(size_t size) {
    if (size == 0) return NULL;

    int idx = size_to_zone_index(size);
    if (idx < 0) return NULL; /* size > 2048 */

    struct slab_zone *z = &zones[idx];

    /* Refill if free list is empty */
    if (!z->free_list) {
        zone_refill(z);
        if (!z->free_list) return NULL; /* PMM exhausted */
    }

    /* Pop head of free list */
    struct slab_chunk *c = z->free_list;
    z->free_list = c->next;
    return (void *)c;
}

void zfree(void *ptr, size_t size) {
    if (!ptr || size == 0) return;

    int idx = size_to_zone_index(size);
    if (idx < 0) return; /* size > 2048, nothing to do */

    struct slab_zone *z = &zones[idx];

    /* Push onto head of free list (LIFO reuse) */
    struct slab_chunk *c = (struct slab_chunk *)ptr;
    c->next = z->free_list;
    z->free_list = c;
}
