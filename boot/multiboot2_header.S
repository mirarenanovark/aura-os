/**
 * @file        boot/multiboot2_header.S
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   BOOT_MB2_HEADER
 * @contract    PRD-07-boot-platform / Multiboot2 Specification
 * @description 8-byte aligned Multiboot2 header loaded by GRUB2 / Limine.
 *              Guarantees bootloader recognition and protected-mode handoff.
 *
 * @connects
 *              - Upstream:   BIOS / UEFI -> GRUB2 Bootloader
 *              - Downstream: boot/entry64.S (_start entry point)
 *              - Hardware:   CPU in 32-bit protected mode (EAX=0x36d76289)
 *
 * @flow        [AURA_FLOW: BOOT_ENTRY]
 *              1. Bootloader verifies magic 0xE85250D6 and checksum.
 *              2. Bootloader loads ELF segments into RAM at 1MB physical.
 *              3. Bootloader jumps to ELF entry point _start in boot/entry64.S.
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
