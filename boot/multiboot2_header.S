/* Multiboot2 header for AuraOS
 * Spec: Magic 0xE85250D6, Architecture 0 (i386/32-bit protected mode), 8-byte aligned.
 */

#define MULTIBOOT2_HEADER_MAGIC         0xE85250D6
#define MULTIBOOT_ARCHITECTURE_I386     0
#define MULTIBOOT_HEADER_TAG_END        0

.section .multiboot, "a"
.align 8
.global multiboot_header_start
multiboot_header_start:
    /* magic */
    .long MULTIBOOT2_HEADER_MAGIC
    /* architecture */
    .long MULTIBOOT_ARCHITECTURE_I386
    /* header_length */
    .long multiboot_header_end - multiboot_header_start
    /* checksum */
    .long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header_start))

    /* End tag */
    .short MULTIBOOT_HEADER_TAG_END
    .short 0
    .long 8
multiboot_header_end:
