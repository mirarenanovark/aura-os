/**
 * @file        kernel/include/aura/zram.h
 * @layer       STAGE_3_VIRTUAL_SCHEDULER
 * @component   MEMORY_ZRAM_COMPRESSOR
 * @contract    PRD-05-resources-telemetry / WKdm-compressed-memory
 * @description Lightweight in-RAM compressed memory pool (zram / vm_compressor).
 *              Compresses dirty 4KB pages in memory using a fast run-length /
 *              dictionary pattern (WKdm-style), avoiding disk thrashing and
 *              effectively expanding usable physical RAM by 2x to 3x.
 */

#ifndef AURA_ZRAM_H
#define AURA_ZRAM_H

#include <stdint.h>
#include <stddef.h>

#define ZRAM_PAGE_SIZE      4096
#define ZRAM_MAX_PAGES     1024  /* 4MB uncompressed capacity in pool */

struct zram_stats {
    uint32_t pages_stored;        /* Count of active compressed pages */
    uint32_t original_bytes;      /* Total uncompressed size of stored pages */
    uint32_t compressed_bytes;    /* Actual RAM consumed by compressed blocks */
    uint32_t compression_ratio_x100; /* e.g. 250 = 2.50x savings */
    uint32_t total_compressions;  /* Lifetime compression operations */
    uint32_t total_decompressions;/* Lifetime decompression operations */
};

/**
 * @brief Initialize zram compressed pool.
 * @return 0 on success, negative on error.
 */
int zram_init(void);

/**
 * @brief Compress a 4KB page and store it in the zram pool.
 * @param src Pointer to 4096-byte page.
 * @param out_handle Output identifier for retrieving the page.
 * @return 0 on success, negative if pool full or incompressible.
 */
int zram_store(const void *src, uint32_t *out_handle);

/**
 * @brief Decompress and restore a 4KB page from zram by handle.
 * @param handle The identifier returned by zram_store.
 * @param dst Pointer to destination buffer (must be at least 4096 bytes).
 * @return 0 on success, negative on invalid handle.
 */
int zram_load(uint32_t handle, void *dst);

/**
 * @brief Free a compressed page slot from zram.
 * @param handle The handle to release.
 * @return 0 on success.
 */
int zram_free(uint32_t handle);

/**
 * @brief Retrieve current zram usage and efficiency metrics.
 * @param out_stats Destination stats structure.
 */
void zram_get_stats(struct zram_stats *out_stats);

/**
 * @brief Fast in-memory page compressor (WKdm/RLE hybrid).
 * @param src 4096 bytes input.
 * @param dst Output buffer (must be >= 4096 bytes).
 * @param max_dst Size of dst.
 * @return Compressed length in bytes, or negative if incompressible.
 */
int zram_compress_page(const uint8_t *src, uint8_t *dst, size_t max_dst);

/**
 * @brief Fast in-memory page decompressor.
 * @param src Compressed stream.
 * @param src_len Length of compressed stream.
 * @param dst Output buffer (4096 bytes).
 * @return 0 on success, negative on corruption.
 */
int zram_decompress_page(const uint8_t *src, size_t src_len, uint8_t *dst);

#endif /* AURA_ZRAM_H */
