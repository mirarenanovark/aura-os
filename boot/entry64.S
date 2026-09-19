/* Entry point for AuraOS x86_64
 * Multiboot2 enters in 32-bit protected mode (EAX=0x36d76289, EBX=multiboot_info).
 * _start sets up 64-bit long mode with identity paging, loads 64-bit GDT, jumps to long mode,
 * and calls kernel_main.
 */

.code32
.section .text
.global _start

_start:
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

    /* Set up identity paging for first 1GB using 2MB huge pages */
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
