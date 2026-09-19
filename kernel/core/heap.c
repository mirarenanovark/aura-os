#include <aura/heap.h>

/*
 * AuraOS Free-List Kernel Heap Allocator
 * Clean. Simple. Small. Fast. Direct.
 *
 * Block layout in memory:
 * [block_t header] [user payload ...] [block_t header] [user payload ...]
 */

typedef struct block {
    size_t size;            /* Payload size in bytes (excludes header) */
    uint8_t used;           /* 1 = allocated, 0 = free */
    struct block *next;     /* Next adjacent block in physical memory */
    struct block *prev;     /* Previous adjacent block in physical memory */
    struct block *next_free;/* Next block in free-list */
    struct block *prev_free;/* Previous block in free-list */
} block_t;

#define HEADER_SIZE sizeof(block_t)
#define MIN_SPLIT   32

static block_t *free_head = NULL;
static size_t heap_used = 0;
static uintptr_t phys_offset = 0;

static inline size_t align_up(size_t val, size_t align) {
    return (val + align - 1) & ~(align - 1);
}

static inline void *phys_to_virt(uintptr_t pa) {
    return (void *)(pa + phys_offset);
}

static void free_list_add(block_t *blk) {
    blk->next_free = free_head;
    blk->prev_free = NULL;
    if (free_head) {
        free_head->prev_free = blk;
    }
    free_head = blk;
}

static void free_list_remove(block_t *blk) {
    if (blk->prev_free) {
        blk->prev_free->next_free = blk->next_free;
    } else {
        free_head = blk->next_free;
    }
    if (blk->next_free) {
        blk->next_free->prev_free = blk->prev_free;
    }
    blk->next_free = NULL;
    blk->prev_free = NULL;
}

static int grow_heap(size_t min_payload) {
    size_t need = min_payload + HEADER_SIZE;
    size_t pages = (need + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t pa = pmm_alloc_contiguous(pages);
    if (!pa) return 0;

    block_t *blk = (block_t *)phys_to_virt(pa);
    blk->size = (pages * PAGE_SIZE) - HEADER_SIZE;
    blk->used = 0;
    blk->next = NULL;
    blk->prev = NULL;
    blk->next_free = NULL;
    blk->prev_free = NULL;

    free_list_add(blk);
    return 1;
}

void heap_init(uintptr_t offset) {
    free_head = NULL;
    heap_used = 0;
    phys_offset = offset;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = align_up(size, 8);

    /* First-fit search */
    block_t *cur = free_head;
    while (cur) {
        if (cur->size >= size) {
            /* Can we split? */
            if (cur->size >= size + HEADER_SIZE + MIN_SPLIT) {
                size_t old_size = cur->size;
                cur->size = size;

                block_t *split = (block_t *)((uint8_t *)(cur + 1) + size);
                split->size = old_size - size - HEADER_SIZE;
                split->used = 0;
                split->next = cur->next;
                split->prev = cur;
                if (cur->next) cur->next->prev = split;
                cur->next = split;

                free_list_add(split);
            }

            free_list_remove(cur);
            cur->used = 1;
            heap_used += cur->size;
            return (void *)(cur + 1);
        }
        cur = cur->next_free;
    }

    /* Grow heap and retry */
    if (!grow_heap(size)) return NULL;
    return kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *blk = ((block_t *)ptr) - 1;
    if (!blk->used) return; /* double-free guard */

    blk->used = 0;
    heap_used -= blk->size;
    free_list_add(blk);

    /* Coalesce right */
    if (blk->next && !blk->next->used) {
        block_t *nxt = blk->next;
        free_list_remove(nxt);
        blk->size += HEADER_SIZE + nxt->size;
        blk->next = nxt->next;
        if (nxt->next) nxt->next->prev = blk;
    }

    /* Coalesce left */
    if (blk->prev && !blk->prev->used) {
        block_t *prv = blk->prev;
        free_list_remove(blk);
        prv->size += HEADER_SIZE + blk->size;
        prv->next = blk->next;
        if (blk->next) blk->next->prev = prv;
    }
}

void *kmalloc_aligned(size_t size, size_t align) {
    if (size == 0 || align == 0) return NULL;
    if ((align & (align - 1)) != 0) return NULL;

    size_t padded = size + align + HEADER_SIZE + sizeof(void *);
    void *raw = kmalloc(padded);
    if (!raw) return NULL;

    uintptr_t addr = (uintptr_t)raw + sizeof(void *);
    uintptr_t aligned = align_up(addr, align);
    ((void **)aligned)[-1] = raw;
    return (void *)aligned;
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    block_t *blk = ((block_t *)ptr) - 1;
    new_size = align_up(new_size, 8);
    if (blk->size >= new_size) return ptr;

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    uint8_t *dst = (uint8_t *)new_ptr;
    uint8_t *src = (uint8_t *)ptr;
    for (size_t i = 0; i < blk->size; i++) dst[i] = src[i];

    kfree(ptr);
    return new_ptr;
}

size_t heap_get_used(void) { return heap_used; }

size_t heap_get_free(void) {
    size_t total = 0;
    block_t *cur = free_head;
    while (cur) {
        total += cur->size;
        cur = cur->next_free;
    }
    return total;
}
