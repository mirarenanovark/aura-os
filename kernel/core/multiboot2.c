/**
 * @file        kernel/core/multiboot2.c
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   BOOT_MULTIBOOT2
 * @contract    PRD-07-boot-platform / Multiboot2 Specification
 * @description Parses the 8-byte aligned Multiboot2 tag stream passed by GRUB2.
 *              Extracts lower/upper basic memory limits, parses physical memory map
 *              (MULTIBOOT_MEMORY_AVAILABLE), and retrieves linear GOP framebuffer tags.
 *
 * @connects
 *              - Upstream:   kernel/main.c:kernel_main (multiboot2_parse)
 *              - Downstream: kernel/core/pmm.c (ram_size sizing)
 *              - Bootloader: mbi_addr pointer passed in EBX by bootloader
 *
 * @flow        [AURA_FLOW: BOOT_PARSING]
 *              1. Validates magic (0x36d76289).
 *              2. Iterates 8-byte aligned tags until MULTIBOOT_TAG_TYPE_END (0).
 *              3. Parses TYPE_BASIC_MEMINFO and TYPE_MMAP to populate RAM metrics.
 *              4. Parses TYPE_FRAMEBUFFER for linear GOP dimensions, pitch, and bpp.
 */

#include <aura/multiboot2.h>
#include <aura/serial.h>

/* Iterate over multiboot2 tag list and extract memory + framebuffer info */
int multiboot2_parse(uint32_t magic, uintptr_t mbi_addr, struct aura_boot_info *out_info) {
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC || !out_info)
        return -1;

    out_info->total_memory_bytes = 0;
    out_info->available_memory_bytes = 0;
    out_info->fb_addr = 0;
    out_info->fb_width = 0;
    out_info->fb_height = 0;
    out_info->fb_pitch = 0;
    out_info->fb_bpp = 0;
    out_info->has_framebuffer = 0;

    /* mbi_addr is 64-bit physical but GRUB guarantees 8-byte alignment */
    struct multiboot_tag *tag = (struct multiboot_tag *)(mbi_addr + 8);

    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        switch (tag->type) {
        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
            /* memory_lower/memory_upper in KiB blocks (1MiB + upper) */
            uint32_t *meminfo = (uint32_t *)(tag + 1);
            out_info->total_memory_bytes = (uint64_t)(meminfo[1] + 1024) * 1024ULL;
            break;
        }

        case MULTIBOOT_TAG_TYPE_MMAP: {
            struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap *)tag;
            uint8_t *entry_ptr = (uint8_t *)tag + sizeof(struct multiboot_tag_mmap);
            uint32_t entries = (tag->size - sizeof(struct multiboot_tag_mmap)) / mmap_tag->entry_size;

            for (uint32_t i = 0; i < entries; i++) {
                struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry *)(entry_ptr + (i * mmap_tag->entry_size));

                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    out_info->available_memory_bytes += entry->len;
                }
            }
            break;
        }

        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
            struct multiboot_tag_framebuffer *fb = (struct multiboot_tag_framebuffer *)tag;
            out_info->fb_addr = fb->framebuffer_addr;
            out_info->fb_width = fb->framebuffer_width;
            out_info->fb_height = fb->framebuffer_height;
            out_info->fb_pitch = fb->framebuffer_pitch;
            out_info->fb_bpp = fb->framebuffer_bpp;
            out_info->has_framebuffer = 1;
            break;
        }

        default:
            break;
        }

        /* Tags are 8-byte aligned */
        tag = (struct multiboot_tag *)((uintptr_t)tag + ((tag->size + 7) & ~7ULL));
    }

    return 0;
}
