#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <aura/zram.h>
#include <aura/heap.h>
#include <aura/pmm.h>

#define TEST_RAM_SIZE (4UL * 1024 * 1024)

int main(void) {
    printf("--- Running test_zram ---\n");

    /* Allocate simulated physical memory block */
    uint8_t *simulated_ram = malloc(TEST_RAM_SIZE);
    assert(simulated_ram != NULL);

    uintptr_t bitmap_base = (uintptr_t)simulated_ram;
    pmm_init(TEST_RAM_SIZE, bitmap_base);

    /* Mark bitmap frames as used */
    size_t total_frames = TEST_RAM_SIZE / 4096;
    size_t bitmap_bytes = (total_frames + 7) / 8;
    size_t bitmap_pages = (bitmap_bytes + 4096 - 1) / 4096;
    for (size_t i = 0; i < bitmap_pages; i++) {
        pmm_mark_used(i * 4096);
    }

    /* Initialize heap with simulated ram offset */
    heap_init((uintptr_t)simulated_ram);

    int init_res = zram_init();
    assert(init_res == 0);

    /* 1. Test compression of zero page */
    uint8_t zero_page[4096];
    memset(zero_page, 0, sizeof(zero_page));

    uint8_t comp_buf[4096];
    int comp_len = zram_compress_page(zero_page, comp_buf, sizeof(comp_buf));
    assert(comp_len > 0);
    assert(comp_len <= 40); /* 4096 zeros pack into 255-byte chunks: 17 x 2 = 34 bytes */

    uint8_t decomp_buf[4096];
    int decomp_res = zram_decompress_page(comp_buf, comp_len, decomp_buf);
    assert(decomp_res == 0);
    assert(memcmp(zero_page, decomp_buf, 4096) == 0);
    printf("[PASS] Zero-page compression: 4096 -> %d bytes\n", comp_len);

    /* 2. Test pattern page (repeated runs that compress) */
    uint8_t pattern_page[4096];
    for (int i = 0; i < 4096; i++) {
        pattern_page[i] = (uint8_t)(i / 64);
    }
    comp_len = zram_compress_page(pattern_page, comp_buf, sizeof(comp_buf));
    assert(comp_len > 0);
    decomp_res = zram_decompress_page(comp_buf, comp_len, decomp_buf);
    assert(decomp_res == 0);
    assert(memcmp(pattern_page, decomp_buf, 4096) == 0);
    printf("[PASS] Pattern-page compression: 4096 -> %d bytes\n", comp_len);

    /* 3. Test zram store / load / free API */
    uint32_t handle = 0;
    int store_res = zram_store(zero_page, &handle);
    assert(store_res == 0);

    struct zram_stats stats;
    zram_get_stats(&stats);
    assert(stats.pages_stored == 1);
    assert(stats.compression_ratio_x100 > 100);
    printf("[PASS] zram_store handle=%u, ratio=%u.%ux\n",
           handle, stats.compression_ratio_x100 / 100, (stats.compression_ratio_x100 % 100) / 10);

    uint8_t restored[4096];
    int load_res = zram_load(handle, restored);
    assert(load_res == 0);
    assert(memcmp(zero_page, restored, 4096) == 0);
    printf("[PASS] zram_load restored correctly\n");

    int free_res = zram_free(handle);
    assert(free_res == 0);
    zram_get_stats(&stats);
    assert(stats.pages_stored == 0);
    printf("[PASS] zram_free released slot cleanly\n");

    free(simulated_ram);
    return 0;
}
