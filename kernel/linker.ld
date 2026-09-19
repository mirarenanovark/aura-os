/**
 * @file        kernel/linker.ld
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   LINKER_SCRIPT
 * @contract    ADR-001 / PRD-07-boot-platform
 * @description Defines physical memory image mapping for the 64-bit AuraOS kernel.
 *              Positions .multiboot at 1MB physical (0x100000), followed by 4KB aligned
 *              .text, .rodata, .data, and .bss segments.
 *
 * @connects
 *              - Upstream:   Makefile (LD linker stage)
 *              - Downstream: boot/entry64.S, kernel/main.c
 *              - Memory:     0x00100000 (1MB) kernel load boundary
 */

ENTRY(_start)

SECTIONS
{
    /* Load at 1MB physical memory */
    . = 1M;

    .multiboot : ALIGN(8)
    {
        KEEP(*(.multiboot))
    }

    .text : ALIGN(4K)
    {
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    /* [AURA_COMPONENT: LINKER_SCRIPT] Export kernel end for dynamic PMM bitmap placement */
    _end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
