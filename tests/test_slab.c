/**
 * @file        tests/test_slab.c
 * @layer       TEST
 * @component   SLAB_ZONE_ALLOCATOR
 * @contract    PRD-05-resources-telemetry
 * @description Host unit tests for the power-of-2 slab zone allocator.
 *              Validates all 8 zone sizes, LIFO reuse, page exhaustion/second
 *              page allocation, and edge cases (size 0, size > 2048).
 *
 * @connects
 *              - Upstream:   make test-slab
 *              - Downstream: kernel/core/slab.c, kernel/core/pmm.c
 */

#include <aura/slab.h>
#include <aura/pmm.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* 4MB simulated RAM for the test harness. */
#define TEST_RAM_SIZE (4UL * 1024 * 1024)

/* Slab uses phys_to_virt internally; in host tests, PMM returns phys addrs
   starting at 0 and we offset them into the simulated_ram buffer. */
extern void slab_set_phys_offset(uintptr_t offset);

/* Helper: fill memory with a pattern */
static void fill(void *ptr, size_t n, uint8_t v) {
    memset(ptr, v, n);
}

/* Helper: verify memory pattern */
static int check(const void *ptr, size_t n, uint8_t v) {
    const uint8_t *p = (const uint8_t *)ptr;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != v) return 0;
    }
    return 1;
}

/* ── Test 1: Basic allocation for all 8 zone sizes ─────────────────────── */

static void test_all_zone_sizes(void) {
    printf("  T1  alloc all 8 zone sizes (16..2048) ... ");

    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    void *ptrs[8];

    for (int i = 0; i < 8; i++) {
        ptrs[i] = zalloc(sizes[i]);
        assert(ptrs[i] != NULL);
        fill(ptrs[i], sizes[i], (uint8_t)(0x10 + i));
        assert(check(ptrs[i], sizes[i], (uint8_t)(0x10 + i)));
    }

    /* All pointers must be distinct */
    for (int i = 0; i < 8; i++) {
        for (int j = i + 1; j < 8; j++) {
            assert(ptrs[i] != ptrs[j]);
        }
    }

    for (int i = 0; i < 8; i++) {
        zfree(ptrs[i], sizes[i]);
    }
    printf("ok\n");
}

/* ── Test 2: LIFO reuse after free ─────────────────────────────────────── */

static void test_lifo_reuse(void) {
    printf("  T2  LIFO reuse after free ... ");

    /* Allocate from the 64-byte zone, free, realloc — should get same ptr */
    void *p = zalloc(64);
    assert(p != NULL);
    uintptr_t addr = (uintptr_t)p;
    zfree(p, 64);

    void *q = zalloc(64);
    assert(q != NULL);
    assert((uintptr_t)q == addr); /* LIFO: same chunk returned */

    zfree(q, 64);
    printf("ok\n");
}

/* ── Test 3: Exhaust a page and trigger refill ─────────────────────────── */

static void test_page_exhaustion(void) {
    printf("  T3  exhaust 4KB page + second page alloc ... ");

    /* 16-byte zone: 4096/16 = 256 chunks per page */
    size_t count = 4096 / 16;
    void *ptrs[260]; /* a few extra to trigger second page */

    for (size_t i = 0; i < count; i++) {
        ptrs[i] = zalloc(16);
        assert(ptrs[i] != NULL);
    }

    /* This should trigger allocation of a second 4KB page */
    ptrs[count] = zalloc(16);
    assert(ptrs[count] != NULL);

    /* All 257 pointers must be distinct */
    for (size_t i = 0; i <= count; i++) {
        for (size_t j = i + 1; j <= count; j++) {
            assert(ptrs[i] != ptrs[j]);
        }
    }

    for (size_t i = 0; i <= count; i++) {
        zfree(ptrs[i], 16);
    }
    printf("ok\n");
}

/* ── Test 4: Edge case — size 0 ────────────────────────────────────────── */

static void test_size_zero(void) {
    printf("  T4  edge: size 0 returns NULL ... ");
    void *p = zalloc(0);
    assert(p == NULL);
    printf("ok\n");
}

/* ── Test 5: Edge case — size > 2048 ──────────────────────────────────── */

static void test_size_too_large(void) {
    printf("  T5  edge: size > 2048 returns NULL ... ");
    void *p1 = zalloc(2049);
    assert(p1 == NULL);
    void *p2 = zalloc(4096);
    assert(p2 == NULL);
    void *p3 = zalloc((size_t)-1);
    assert(p3 == NULL);
    printf("ok\n");
}

/* ── Test 6: Mixed-zone stress ─────────────────────────────────────────── */

static void test_mixed_stress(void) {
    printf("  T6  mixed-zone stress (100 allocs across zones) ... ");

    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    void *ptrs[100];

    for (int i = 0; i < 100; i++) {
        size_t sz = sizes[i % 8];
        ptrs[i] = zalloc(sz);
        assert(ptrs[i] != NULL);
        fill(ptrs[i], sz, (uint8_t)i);
    }

    for (int i = 0; i < 100; i++) {
        size_t sz = sizes[i % 8];
        assert(check(ptrs[i], sz, (uint8_t)i));
    }

    for (int i = 0; i < 100; i++) {
        size_t sz = sizes[i % 8];
        zfree(ptrs[i], sz);
    }
    printf("ok\n");
}

/* ── Test 7: zfree with NULL is safe ───────────────────────────────────── */

static void test_null_free(void) {
    printf("  T7  zfree(NULL, size) is safe ... ");
    zfree(NULL, 64);
    zfree(NULL, 0);
    printf("ok\n");
}

/* ── Main ──────────────────────────────────────────────────────────────── */

int main(void) {
    printf("Running Slab Allocator Tests (Simulated %lu KB RAM)...\n",
           (unsigned long)(TEST_RAM_SIZE / 1024));

    /* Allocate simulated physical RAM */
    uint8_t *simulated_ram = malloc(TEST_RAM_SIZE);
    assert(simulated_ram != NULL);

    /* Initialize PMM with bitmap at start of simulated RAM */
    uintptr_t bitmap_base = (uintptr_t)simulated_ram;
    pmm_init(TEST_RAM_SIZE, bitmap_base);

    /* Mark bitmap region as used so PMM won't hand it out */
    size_t total_frames = TEST_RAM_SIZE / PAGE_SIZE;
    size_t bitmap_bytes = (total_frames + 7) / 8;
    size_t bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < bitmap_pages; i++) {
        pmm_mark_used(i * PAGE_SIZE);
    }

    /* Set slab phys-to-virt offset so PMM phys addrs map into simulated_ram */
    slab_set_phys_offset((uintptr_t)simulated_ram);
    slab_init();

    test_all_zone_sizes();
    test_lifo_reuse();
    test_page_exhaustion();
    test_size_zero();
    test_size_too_large();
    test_mixed_stress();
    test_null_free();

    free(simulated_ram);
    printf("All Slab Tests Passed.\n");
    return 0;
}
