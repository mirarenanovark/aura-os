/**
 * @file        boot/entry64.S
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   BOOT_ENTRY64
 * @contract    PRD-07-boot-platform / ADR-001
 * @description 32-bit Multiboot2 kernel entry point. Validates CPU long mode support,
 *              constructs initial 1GB 2MB huge identity paging, transitions CPU into
 *              64-bit Long Mode, and hands off execution to kernel_main().
 *
 * @connects
 *              - Upstream:   boot/multiboot2_header.S -> GRUB2 / Limine
 *              - Downstream: kernel/main.c (kernel_main)
 *              - Hardware:   CPU CR0, CR3, CR4, EFER MSR (0xC0000080), CPUID
 *
 * @flow        [AURA_FLOW: BOOT_ENTRY]
 *              1. Save Multiboot2 magic (EAX) and MBI pointer (EBX).
 *              2. Verify CPUID availability and Long Mode support (EDX bit 29).
 *              3. Clear and construct 1GB 2MB-huge page identity map (PML4->PDPT->PD).
 *              4. Load PML4 base into CR3, enable PAE (CR4.PAE), enable LME in EFER MSR.
 *              5. Enable Paging & Protected Mode (CR0.PG | CR0.PE).
 *              6. Load temporary 64-bit boot GDT, perform far jump to long_mode_start.
 *              7. Reload segment selectors (0x10), setup 16KB stack, pass args to kernel_main().
 */

.code32
.section .text
.global _start

_start:
    // [AURA_FLOW: BOOT_ENTRY] Step 1: Initialize CPU flags & save bootloader registers
    cli
    cld

    /* Force reference to multiboot header so linker doesn't drop it */
    movl $multiboot_header_start, %edx

    /* Save multiboot magic and info pointer */
    movl %eax, (multiboot_magic)
    movl %ebx, (multiboot_info)

    /* Set up stack */
    movl $stack_top, %esp

    /* Check if CPUID is supported */
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $(1 << 21), %eax
    pushl %eax
    popfl
    pushfl
    popl %eax
    pushl %ecx
    popfl
    cmpl %ecx, %eax
    je .no_long_mode

    /* Check for extended processor info (CPUID function 0x80000000) */
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb .no_long_mode

    /* Check for long mode support (CPUID function 0x80000001 bit 29 in EDX) */
    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx
    jz .no_long_mode

    // [AURA_FLOW: BOOT_ENTRY] Step 2: Build 1GB 2MB huge identity paging (PML4, PDPT, PD)
    /* Clear page tables (4KB each for PML4, PDPT, PD) */
    movl $boot_pml4, %edi
    movl $0, %eax
    movl $3072, %ecx        /* 3 pages * 1024 dwords = 3072 */
    rep stosl

    /* Link PML4[0] -> boot_pdpt (Present, Writable = 0x03) */
    movl $boot_pdpt, %eax
    orl $0x03, %eax
    movl %eax, (boot_pml4)

    /* Link PDPT[0] -> boot_pd (Present, Writable = 0x03) */
    movl $boot_pd, %eax
    orl $0x03, %eax
    movl %eax, (boot_pdpt)

    /* Fill PD with 512 entries of 2MB huge pages (0x83 = Present, Writable, Huge Page 2MB) -> 1GB total */
    movl $0, %ecx           /* Entry index 0..511 */
    movl $0x83, %eax        /* Page physical address 0 + flags */
.fill_pd:
    movl %eax, boot_pd(, %ecx, 8)
    movl $0, boot_pd + 4(, %ecx, 8)
    addl $0x200000, %eax    /* +2MB */
    incl %ecx
    cmpl $512, %ecx
    jne .fill_pd

    // [AURA_FLOW: BOOT_ENTRY] Step 3: Enable CR4.PAE, EFER.LME, CR0.PG and load CR3
    /* Load PML4 address into CR3 */
    movl $boot_pml4, %eax
    movl %eax, %cr3

    /* Enable PAE in CR4 (bit 5) */
    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4

    /* Enable Long Mode in EFER MSR (0xC0000080, bit 8) */
    movl $0xC0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr

    /* Enable Paging and Protected Mode in CR0 (bit 31 PG, bit 0 PE) */
    movl %cr0, %eax
    orl $((1 << 31) | (1 << 0)), %eax
    movl %eax, %cr0

    // [AURA_FLOW: BOOT_ENTRY] Step 4: Load 64-bit GDT & far jump to 64-bit Long Mode
    /* Load 64-bit GDT */
    lgdt (boot_gdt64_ptr)

    /* Far jump to 64-bit code */
    ljmp $0x08, $long_mode_start

.no_long_mode:
    /* If CPU does not support 64-bit mode, hang */
    cli
.hang:
    hlt
    jmp .hang

.code64
long_mode_start:
    // [AURA_FLOW: BOOT_ENTRY] Step 5: Transition into Long Mode, reload segments & jump to kernel_main
    /* Reload data segment registers */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    /* Setup 64-bit stack */
    movq $stack_top, %rsp

    /* Pass Multiboot arguments to kernel_main:
     * RDI = multiboot_info (first argument in System V AMD64 ABI)
     * RSI = multiboot_magic (second argument)
     */
    movl (multiboot_info), %edi
    movl (multiboot_magic), %esi

    // [AURA_CONNECTS: KERNEL_ORCHESTRATOR -> kernel_main]
    call kernel_main

    /* If kernel_main returns, halt */
.loop:
    cli
    hlt
    jmp .loop

.section .rodata
.align 8
boot_gdt64:
    .quad 0x0000000000000000 /* Null Descriptor */
    .quad 0x00209A0000000000 /* 64-bit Code Segment (Ring 0, Exec/Read, L=1) */
    .quad 0x0000920000000000 /* 64-bit Data Segment (Ring 0, Read/Write) */
boot_gdt64_ptr:
    .short . - boot_gdt64 - 1
    .quad boot_gdt64

.section .bss
.align 4096
boot_pml4:
    .skip 4096
boot_pdpt:
    .skip 4096
boot_pd:
    .skip 4096

.align 16
stack_bottom:
    .skip 16384 /* 16KB stack */
stack_top:

multiboot_magic:
    .long 0
multiboot_info:
    .long 0
