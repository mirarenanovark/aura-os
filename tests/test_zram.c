#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <aura/zram.h>
#include <aura/heap.h>

int main(void) {
    printf("--- Running test_zram ---\n");

    /* Initialize heap and zram */
    heap_init(0);
    int init_res = zram_init();
    assert(init_res == 0);

    /* 1. Test compression of zero page */
    uint8_t zero_page[4096];
    memset(zero_page, 0, sizeof(zero_page));

    uint8_t comp_buf[4096];
    int comp_len = zram_compress_page(zero_page, comp_buf, sizeof(comp_buf));
    assert(comp_len > 0);
    assert(comp_len < 100); /* Zeros compress to very few bytes */

    uint8_t decomp_buf[4096];
    int decomp_res = zram_decompress_page(comp_buf, comp_len, decomp_buf);
    assert(decomp_res == 0);
    assert(memcmp(zero_page, decomp_buf, 4096) == 0);
    printf("[PASS] Zero-page compression: 4096 -> %d bytes\n", comp_len);

    /* 2. Test pattern page */
    uint8_t pattern_page[4096];
    for (int i = 0; i < 4096; i++) {
        pattern_page[i] = (uint8_t)(i % 16);
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

    printf("--- All zRAM tests passed successfully! ---\n");
    return 0;
}
