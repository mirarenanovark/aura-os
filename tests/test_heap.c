/*
 * Heap allocator unit tests – runs on the host (userspace) using the same
 * source files that ship in the kernel, linked against libc for the test
 * harness (printf, assert, malloc to simulate PMM backing memory).
 *
 * Compile:  gcc -Ikernel/include tests/test_heap.c kernel/core/heap.c \
 *               kernel/core/pmm.c -o build/test_heap && ./build/test_heap
 */

#include <aura/heap.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* 4MB simulated RAM for the test harness. */
#define TEST_RAM_SIZE (4UL * 1024 * 1024)

/* ── Helpers ────────────────────────────────────────────────────────────── */

static void fill(void *ptr, size_t n, uint8_t v) {
    memset(ptr, v, n);
}

static int check(const void *ptr, size_t n, uint8_t v) {
    const uint8_t *p = (const uint8_t *)ptr;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != v) return 0;
    }
    return 1;
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

static void test_basic_alloc(void) {
    printf("  T1  basic alloc/write/free ... ");
    void *p = kmalloc(128);
    assert(p != NULL);
    fill(p, 128, 0xAB);
    assert(check(p, 128, 0xAB));
    kfree(p);
    printf("ok\n");
}

static void test_multi_alloc(void) {
    printf("  T2  multi alloc ... ");
    void *a = kmalloc(64);
    void *b = kmalloc(256);
    void *c = kmalloc(1024);
    assert(a && b && c);
    assert(a != b && b != c && a != c);
    fill(a, 64, 0x11);
    fill(b, 256, 0x22);
    fill(c, 1024, 0x33);
    assert(check(a, 64, 0x11));
    assert(check(b, 256, 0x22));
    assert(check(c, 1024, 0x33));
    kfree(b);
    kfree(a);
    kfree(c);
    printf("ok\n");
}

static void test_reuse(void) {
    printf("  T3  free + reuse ... ");
    void *p = kmalloc(512);
    assert(p != NULL);
    fill(p, 512, 0xCC);
    uintptr_t addr = (uintptr_t)p;
    kfree(p);
    void *q = kmalloc(512);
    assert(q != NULL);
    assert((uintptr_t)q == addr);
    kfree(q);
    printf("ok\n");
}

static void test_merge(void) {
    printf("  T4  merge adjacent free blocks ... ");
    void *a = kmalloc(256);
    void *b = kmalloc(256);
    assert(a && b);
    kfree(b);
    kfree(a);
    /* After merging, a single 512-byte alloc should succeed
       without growing the heap (reuses the merged block). */
    void *big = kmalloc(512);
    assert(big != NULL);
    kfree(big);
    printf("ok\n");
}

static void test_edge_cases(void) {
    printf("  T5  edge cases (zero/NULL) ... ");
    void *p = kmalloc(0);
    assert(p == NULL);
    kfree(NULL);
    kfree(NULL);
    printf("ok\n");
}

static void test_stress(void) {
    printf("  T6  stress: 100 x 32-byte allocs ... ");
    void *ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = kmalloc(32);
        assert(ptrs[i] != NULL);
        fill(ptrs[i], 32, (uint8_t)i);
    }
    for (int i = 0; i < 100; i++) {
        assert(check(ptrs[i], 32, (uint8_t)i));
    }
    for (int i = 0; i < 100; i++) {
        kfree(ptrs[i]);
    }
    printf("ok\n");
}

static void test_aligned(void) {
    printf("  T7  kmalloc_aligned ... ");
    void *p = kmalloc_aligned(100, 4096);
    assert(p != NULL);
    assert(((uintptr_t)p & 4095) == 0);
    fill(p, 100, 0xDD);
    assert(check(p, 100, 0xDD));
    kfree(p);

    void *q = kmalloc_aligned(64, 64);
    assert(q != NULL);
    assert(((uintptr_t)q & 63) == 0);
    kfree(q);
    printf("ok\n");
}

static void test_realloc(void) {
    printf("  T8  krealloc grow/shrink ... ");
    void *p = kmalloc(128);
    assert(p);
    fill(p, 128, 0xEE);

    void *q = krealloc(p, 512);
    assert(q);
    assert(check(q, 128, 0xEE));

    void *r = krealloc(q, 64);
    assert(r);
    assert(check(r, 64, 0xEE));
    kfree(r);

    void *s = krealloc(NULL, 32);
    assert(s);
    kfree(s);

    void *t = kmalloc(16);
    assert(t);
    void *u = krealloc(t, 0);
    assert(u == NULL);
    printf("ok\n");
}

static void test_accounting(void) {
    printf("  T9  heap accounting ... ");
    size_t used_before = heap_get_used();
    void *p = kmalloc(1024);
    assert(p);
    assert(heap_get_used() >= used_before + 1024);
    kfree(p);
    assert(heap_get_used() <= used_before);
    printf("ok\n");
}

static void test_double_free(void) {
    printf("  T10 double-free guard ... ");
    void *p = kmalloc(64);
    assert(p);
    kfree(p);
    kfree(p);  /* second free must be a no-op */
    void *q = kmalloc(64);
    assert(q);
    kfree(q);
    printf("ok\n");
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void) {
    printf("Running Heap Allocator Tests (Simulated %lu KB RAM)...\n",
           (unsigned long)(TEST_RAM_SIZE / 1024));

    /* Simulate physical RAM with a host malloc block.
     * The bitmap sits at the very start; we mark those frames as used
     * so PMM starts allocating from after the bitmap.  We then pass
     * the simulated_ram base as the phys_offset so PMM addresses
     * (0, PAGE_SIZE, ...) translate into real host pointers. */
    uint8_t *simulated_ram = malloc(TEST_RAM_SIZE);
    assert(simulated_ram != NULL);

    uintptr_t bitmap_base = (uintptr_t)simulated_ram;
    pmm_init(TEST_RAM_SIZE, bitmap_base);

    /* Mark bitmap region as used so PMM won't hand it out.
     * Bitmap size = total_frames / 8 bytes, rounded up to a page. */
    size_t total_frames = TEST_RAM_SIZE / PAGE_SIZE;
    size_t bitmap_bytes = (total_frames + 7) / 8;
    size_t bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < bitmap_pages; i++) {
        pmm_mark_used(i * PAGE_SIZE);  /* mark as used in PMM */
    }

    /* Phys-to-virt offset: PMM returns addresses 0..RAM_SIZE,
     * we need them shifted into the host buffer. */
    heap_init((uintptr_t)simulated_ram);

    test_basic_alloc();
    test_multi_alloc();
    test_reuse();
    test_merge();
    test_edge_cases();
    test_stress();
    test_aligned();
    test_realloc();
    test_accounting();
    test_double_free();

    free(simulated_ram);
    printf("All Heap Tests Passed.\n");
    return 0;
}
