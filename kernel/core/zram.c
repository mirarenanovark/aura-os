/**
 * @file        kernel/core/zram.c
 * @layer       STAGE_3_VIRTUAL_SCHEDULER
 * @component   MEMORY_ZRAM_COMPRESSOR
 * @contract    PRD-05-resources-telemetry / WKdm-compressed-memory
 * @description Lightweight in-memory page compressor (zram / Apple vm_compressor style).
 *              Stores compressed anonymous pages in-RAM without disk I/O, leveraging
 *              fast byte-level run-length & zero-word packing optimized for OS memory dumps.
 */

#include <aura/zram.h>
#include <aura/heap.h>
#include <aura/pmm.h>

#define ZRAM_TAG_LITERAL 0x00
#define ZRAM_TAG_ZEROS   0x01
#define ZRAM_TAG_RUN     0x02

struct zram_slot {
    uint8_t *data;
    uint16_t compressed_len;
    uint8_t  used;
};

static struct zram_slot pool[ZRAM_MAX_PAGES];
static struct zram_stats stats;

int zram_compress_page(const uint8_t *src, uint8_t *dst, size_t max_dst) {
    size_t in_idx = 0;
    size_t out_idx = 0;

    while (in_idx < ZRAM_PAGE_SIZE) {
        if (out_idx + 4 >= max_dst) {
            return -1; /* Incompressible or buffer exceeded */
        }

        /* Check for run of zeros */
        if (src[in_idx] == 0) {
            size_t run = 0;
            while (in_idx + run < ZRAM_PAGE_SIZE && src[in_idx + run] == 0 && run < 255) {
                run++;
            }
            if (run >= 4) {
                dst[out_idx++] = ZRAM_TAG_ZEROS;
                dst[out_idx++] = (uint8_t)run;
                in_idx += run;
                continue;
            }
        }

        /* Check for repeated byte run */
        uint8_t val = src[in_idx];
        size_t run = 0;
        while (in_idx + run < ZRAM_PAGE_SIZE && src[in_idx + run] == val && run < 255) {
            run++;
        }
        if (run >= 6) {
            dst[out_idx++] = ZRAM_TAG_RUN;
            dst[out_idx++] = (uint8_t)run;
            dst[out_idx++] = val;
            in_idx += run;
            continue;
        }

        /* Emit literal block */
        size_t lit_start = in_idx;
        size_t lit_len = 0;
        while (in_idx < ZRAM_PAGE_SIZE && lit_len < 127) {
            if (in_idx + 4 < ZRAM_PAGE_SIZE && src[in_idx] == 0 && src[in_idx+1] == 0 &&
                src[in_idx+2] == 0 && src[in_idx+3] == 0) {
                break;
            }
            lit_len++;
            in_idx++;
        }

        if (lit_len > 0) {
            if (out_idx + 1 + lit_len >= max_dst) return -1;
            dst[out_idx++] = (uint8_t)(ZRAM_TAG_LITERAL | (lit_len & 0x7F));
            for (size_t k = 0; k < lit_len; k++) {
                dst[out_idx++] = src[lit_start + k];
            }
        }
    }

    return (int)out_idx;
}

int zram_decompress_page(const uint8_t *src, size_t src_len, uint8_t *dst) {
    size_t in_idx = 0;
    size_t out_idx = 0;

    while (in_idx < src_len && out_idx < ZRAM_PAGE_SIZE) {
        uint8_t tag = src[in_idx++];
        if (tag == ZRAM_TAG_ZEROS) {
            if (in_idx >= src_len) return -1;
            uint8_t count = src[in_idx++];
            for (uint8_t i = 0; i < count && out_idx < ZRAM_PAGE_SIZE; i++) {
                dst[out_idx++] = 0;
            }
        } else if (tag == ZRAM_TAG_RUN) {
            if (in_idx + 1 >= src_len) return -1;
            uint8_t count = src[in_idx++];
            uint8_t val = src[in_idx++];
            for (uint8_t i = 0; i < count && out_idx < ZRAM_PAGE_SIZE; i++) {
                dst[out_idx++] = val;
            }
        } else {
            /* Literal chunk */
            uint8_t lit_len = tag & 0x7F;
            if (lit_len == 0 || in_idx + lit_len > src_len) return -1;
            for (uint8_t i = 0; i < lit_len && out_idx < ZRAM_PAGE_SIZE; i++) {
                dst[out_idx++] = src[in_idx++];
            }
        }
    }

    while (out_idx < ZRAM_PAGE_SIZE) {
        dst[out_idx++] = 0;
    }
    return 0;
}

int zram_init(void) {
    for (size_t i = 0; i < ZRAM_MAX_PAGES; i++) {
        pool[i].data = (void*)0;
        pool[i].compressed_len = 0;
        pool[i].used = 0;
    }

    stats.pages_stored = 0;
    stats.original_bytes = 0;
    stats.compressed_bytes = 0;
    stats.compression_ratio_x100 = 100;
    stats.total_compressions = 0;
    stats.total_decompressions = 0;

    return 0;
}

int zram_store(const void *src, uint32_t *out_handle) {
    if (!src || !out_handle) return -1;

    /* Find free slot */
    int slot = -1;
    for (size_t i = 0; i < ZRAM_MAX_PAGES; i++) {
        if (!pool[i].used) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0) return -2; /* Pool full */

    uint8_t temp_buf[ZRAM_PAGE_SIZE];
    int comp_len = zram_compress_page((const uint8_t *)src, temp_buf, sizeof(temp_buf));
    if (comp_len <= 0 || comp_len >= ZRAM_PAGE_SIZE - 64) {
        return -3; /* Incompressible */
    }

    /* Allocate compressed payload from heap */
    uint8_t *heap_buf = (uint8_t *)kmalloc((size_t)comp_len);
    if (!heap_buf) return -4;

    for (int i = 0; i < comp_len; i++) {
        heap_buf[i] = temp_buf[i];
    }

    pool[slot].data = heap_buf;
    pool[slot].compressed_len = (uint16_t)comp_len;
    pool[slot].used = 1;

    stats.pages_stored++;
    stats.original_bytes += ZRAM_PAGE_SIZE;
    stats.compressed_bytes += (uint32_t)comp_len;
    stats.total_compressions++;
    if (stats.compressed_bytes > 0) {
        stats.compression_ratio_x100 = (uint32_t)(((uint64_t)stats.original_bytes * 100ULL) / stats.compressed_bytes);
    }

    *out_handle = (uint32_t)slot;
    return 0;
}

int zram_load(uint32_t handle, void *dst) {
    if (handle >= ZRAM_MAX_PAGES || !pool[handle].used || !dst) return -1;

    int ret = zram_decompress_page(pool[handle].data, (size_t)pool[handle].compressed_len, (uint8_t *)dst);
    if (ret == 0) {
        stats.total_decompressions++;
    }
    return ret;
}

int zram_free(uint32_t handle) {
    if (handle >= ZRAM_MAX_PAGES || !pool[handle].used) return -1;

    stats.pages_stored--;
    stats.original_bytes -= ZRAM_PAGE_SIZE;
    stats.compressed_bytes -= pool[handle].compressed_len;
    if (stats.compressed_bytes > 0) {
        stats.compression_ratio_x100 = (uint32_t)(((uint64_t)stats.original_bytes * 100ULL) / stats.compressed_bytes);
    } else {
        stats.compression_ratio_x100 = 100;
    }

    kfree(pool[handle].data);
    pool[handle].data = (void*)0;
    pool[handle].compressed_len = 0;
    pool[handle].used = 0;
    return 0;
}

void zram_get_stats(struct zram_stats *out_stats) {
    if (out_stats) {
        *out_stats = stats;
    }
}
