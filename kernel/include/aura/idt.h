/**
 * @file        kernel/include/aura/idt.h
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   ARCH_IDT
 * @contract    PRD-07-boot-platform
 * @description Interrupt Descriptor Table structures, saved interrupt frame
 *              definitions, and handler registration prototypes.
 *
 * @connects
 *              - Upstream:   kernel/main.c, pit.c, idt.c, isr.S
 *              - Hardware:   CPU IDTR register, 8259 PIC (0x20/0x21, 0xA0/0xA1)
 */

#ifndef AURA_IDT_H
#define AURA_IDT_H

#include <stdint.h>

/* CPU registers saved on interrupt */
#if defined(__x86_64__)
struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));
#else
struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed));
#endif

typedef struct registers registers_t;
typedef void (*isr_handler_t)(registers_t *);

/* IDT entry */
#if defined(__x86_64__)
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  ist;        /* bits 0..2 holds IST index, rest 0 */
    uint8_t  flags;      /* type and attributes */
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));
#else
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));
#endif

/* IDT pointer */
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);
void isr_register_handler(uint8_t num, isr_handler_t handler);
void isr_install(void);

#endif /* AURA_IDT_H */
