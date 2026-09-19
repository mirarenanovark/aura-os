/**
 * @file        kernel/include/aura/multiboot2.h
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   BOOT_MULTIBOOT2
 * @contract    PRD-07-boot-platform / Multiboot2 Specification
 * @description Multiboot2 tag structures, memory type enumerations,
 *              linear GOP framebuffer structure, and parse prototype.
 *
 * @connects
 *              - Upstream:   kernel/main.c, kernel/core/multiboot2.c
 *              - Bootloader: GRUB2 / Limine tag stream
 */

#ifndef AURA_MULTIBOOT2_H
#define AURA_MULTIBOOT2_H

#include <stdint.h>
#include <stddef.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC    0x36d76289

#define MULTIBOOT_TAG_TYPE_END         0
#define MULTIBOOT_TAG_TYPE_CMDLINE     1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME 2
#define MULTIBOOT_TAG_TYPE_MODULE      3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO 4
#define MULTIBOOT_TAG_TYPE_BOOTDEV     5
#define MULTIBOOT_TAG_TYPE_MMAP        6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8

#define MULTIBOOT_MEMORY_AVAILABLE     1
#define MULTIBOOT_MEMORY_RESERVED      2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS           4
#define MULTIBOOT_MEMORY_BADRAM        5

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
};

struct aura_boot_info {
    uint64_t total_memory_bytes;
    uint64_t available_memory_bytes;
    
    /* Framebuffer info if provided by bootloader */
    uint64_t fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint8_t  fb_bpp;
    uint8_t  has_framebuffer;
};

int multiboot2_parse(uint32_t magic, uintptr_t mbi_addr, struct aura_boot_info *out_info);

#endif /* AURA_MULTIBOOT2_H */
