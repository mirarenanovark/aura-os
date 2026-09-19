/**
 * @file        kernel/arch/x86_shared/idt.c
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   ARCH_IDT
 * @contract    PRD-07-boot-platform
 * @description Configures the 256-gate Interrupt Descriptor Table (IDT),
 *              remaps the dual 8259 PIC (Master: 0x20, Slave: 0x28) to avoid
 *              CPU exception conflicts, and installs exception/IRQ dispatchers.
 *
 * @connects
 *              - Upstream:   kernel/main.c:kernel_main (isr_install)
 *              - Downstream: kernel/arch/x86_shared/isr.S, pit.c, serial.c
 *              - Hardware:   CPU IDTR register, Master PIC (0x20/0x21), Slave PIC (0xA0/0xA1)
 *
 * @flow        [AURA_FLOW: IDT_SETUP]
 *              1. isr_install(): Remaps PIC vectors 0-15 to 32-47 via ICW1-ICW4.
 *              2. Populates 32 CPU exception gates with isr0..isr31 stubs.
 *              3. Populates 16 hardware IRQ gates with irq0..irq15 stubs.
 *              4. Loads IDTR using inline assembly 'lidt'.
 *              5. isr_handler() / irq_handler(): Dispatches to C callbacks and sends EOI.
 */

#include <aura/idt.h>
#include <aura/gdt.h>
#include <aura/serial.h>
#include <stdint.h>
#include <stddef.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01

#define PIC_EOI      0x20

static struct idt_entry idt[256];
static struct idt_ptr   idtp;
static isr_handler_t    interrupt_handlers[256];

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags)
{
#if defined(__x86_64__)
    idt[num].base_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].selector  = selector;
    idt[num].ist       = 0;
    idt[num].flags     = flags;
    idt[num].base_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].base_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].reserved  = 0;
#else
    idt[num].base_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].selector  = selector;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
    idt[num].base_high = (uint16_t)((handler >> 16) & 0xFFFF);
#endif
}

void isr_register_handler(uint8_t num, isr_handler_t handler)
{
    interrupt_handlers[num] = handler;
}

static void pic_remap(int offset1, int offset2)
{
    /* ICW1: start initialization sequence in cascade mode */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: master/slave vector offsets */
    outb(PIC1_DATA, (uint8_t)offset1);
    io_wait();
    outb(PIC2_DATA, (uint8_t)offset2);
    io_wait();

    /* ICW3: tell Master PIC there is a slave at IRQ2 (0000 0100) */
    outb(PIC1_DATA, 0x04);
    io_wait();
    /* tell Slave PIC its cascade identity (0000 0010) */
    outb(PIC2_DATA, 0x02);
    io_wait();

    /* ICW4: 8086/88 (MCS-80/85) mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Mask all IRQs except PIT (IRQ0), Keyboard (IRQ1), and cascade (IRQ2) on Master.
     * Mask all IRQs on Slave.
     * Master: bit 0 is IRQ0, bit 1 is IRQ1, bit 2 is IRQ2.
     * Mask register: 0 = unmasked (enabled), 1 = masked (disabled).
     * We want IRQ0, IRQ1, and IRQ2 unmasked: ~(0x01 | 0x02 | 0x04) = ~0x07 = 0xF8
     */
    outb(PIC1_DATA, 0xF8);
    outb(PIC2_DATA, 0xFF);
}

static void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

/* Declare all assembly stubs */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

static const char *exception_messages[32] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

void isr_handler(registers_t *regs)
{
    if (regs->int_no < 32) {
        serial_printf(COM1, "\n[KERNEL PANIC] CPU Exception %d: %s (err code: 0x%x)\n",
                      (uint32_t)regs->int_no,
                      exception_messages[regs->int_no],
                      (uint32_t)regs->err_code);
#if defined(__x86_64__)
        serial_printf(COM1, "RIP: 0x%x  CS: 0x%x  RFLAGS: 0x%x  RSP: 0x%x\n",
                      regs->rip, regs->cs, regs->rflags, regs->rsp);
        serial_printf(COM1, "RAX: 0x%x  RBX: 0x%x  RCX: 0x%x  RDX: 0x%x\n",
                      regs->rax, regs->rbx, regs->rcx, regs->rdx);
#endif
        for (;;) {
            __asm__ volatile("cli; hlt");
        }
    }

    if (interrupt_handlers[regs->int_no]) {
        interrupt_handlers[regs->int_no](regs);
    }
}

void irq_handler(registers_t *regs)
{
    if (interrupt_handlers[regs->int_no]) {
        interrupt_handlers[regs->int_no](regs);
    }

    pic_send_eoi((uint8_t)(regs->int_no - 32));
}

void isr_install(void)
{
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint64_t)(uintptr_t)&idt;

    uint8_t *idt_bytes = (uint8_t *)&idt;
    for (size_t i = 0; i < sizeof(idt); i++) {
        idt_bytes[i] = 0;
    }
    for (int i = 0; i < 256; i++) {
        interrupt_handlers[i] = NULL;
    }

    /* Remap PIC to 0x20 and 0x28 */
    pic_remap(0x20, 0x28);

    /* 0x8E = Present, Ring 0, 64-bit Interrupt Gate */
    uint8_t flags = 0x8E;
    uint16_t sel = GDT_KERNEL_CODE;

    idt_set_gate(0,  (uint64_t)(uintptr_t)isr0,  sel, flags);
    idt_set_gate(1,  (uint64_t)(uintptr_t)isr1,  sel, flags);
    idt_set_gate(2,  (uint64_t)(uintptr_t)isr2,  sel, flags);
    idt_set_gate(3,  (uint64_t)(uintptr_t)isr3,  sel, flags);
    idt_set_gate(4,  (uint64_t)(uintptr_t)isr4,  sel, flags);
    idt_set_gate(5,  (uint64_t)(uintptr_t)isr5,  sel, flags);
    idt_set_gate(6,  (uint64_t)(uintptr_t)isr6,  sel, flags);
    idt_set_gate(7,  (uint64_t)(uintptr_t)isr7,  sel, flags);
    idt_set_gate(8,  (uint64_t)(uintptr_t)isr8,  sel, flags);
    idt_set_gate(9,  (uint64_t)(uintptr_t)isr9,  sel, flags);
    idt_set_gate(10, (uint64_t)(uintptr_t)isr10, sel, flags);
    idt_set_gate(11, (uint64_t)(uintptr_t)isr11, sel, flags);
    idt_set_gate(12, (uint64_t)(uintptr_t)isr12, sel, flags);
    idt_set_gate(13, (uint64_t)(uintptr_t)isr13, sel, flags);
    idt_set_gate(14, (uint64_t)(uintptr_t)isr14, sel, flags);
    idt_set_gate(15, (uint64_t)(uintptr_t)isr15, sel, flags);
    idt_set_gate(16, (uint64_t)(uintptr_t)isr16, sel, flags);
    idt_set_gate(17, (uint64_t)(uintptr_t)isr17, sel, flags);
    idt_set_gate(18, (uint64_t)(uintptr_t)isr18, sel, flags);
    idt_set_gate(19, (uint64_t)(uintptr_t)isr19, sel, flags);
    idt_set_gate(20, (uint64_t)(uintptr_t)isr20, sel, flags);
    idt_set_gate(21, (uint64_t)(uintptr_t)isr21, sel, flags);
    idt_set_gate(22, (uint64_t)(uintptr_t)isr22, sel, flags);
    idt_set_gate(23, (uint64_t)(uintptr_t)isr23, sel, flags);
    idt_set_gate(24, (uint64_t)(uintptr_t)isr24, sel, flags);
    idt_set_gate(25, (uint64_t)(uintptr_t)isr25, sel, flags);
    idt_set_gate(26, (uint64_t)(uintptr_t)isr26, sel, flags);
    idt_set_gate(27, (uint64_t)(uintptr_t)isr27, sel, flags);
    idt_set_gate(28, (uint64_t)(uintptr_t)isr28, sel, flags);
    idt_set_gate(29, (uint64_t)(uintptr_t)isr29, sel, flags);
    idt_set_gate(30, (uint64_t)(uintptr_t)isr30, sel, flags);
    idt_set_gate(31, (uint64_t)(uintptr_t)isr31, sel, flags);

    idt_set_gate(32, (uint64_t)(uintptr_t)irq0,  sel, flags);
    idt_set_gate(33, (uint64_t)(uintptr_t)irq1,  sel, flags);
    idt_set_gate(34, (uint64_t)(uintptr_t)irq2,  sel, flags);
    idt_set_gate(35, (uint64_t)(uintptr_t)irq3,  sel, flags);
    idt_set_gate(36, (uint64_t)(uintptr_t)irq4,  sel, flags);
    idt_set_gate(37, (uint64_t)(uintptr_t)irq5,  sel, flags);
    idt_set_gate(38, (uint64_t)(uintptr_t)irq6,  sel, flags);
    idt_set_gate(39, (uint64_t)(uintptr_t)irq7,  sel, flags);
    idt_set_gate(40, (uint64_t)(uintptr_t)irq8,  sel, flags);
    idt_set_gate(41, (uint64_t)(uintptr_t)irq9,  sel, flags);
    idt_set_gate(42, (uint64_t)(uintptr_t)irq10, sel, flags);
    idt_set_gate(43, (uint64_t)(uintptr_t)irq11, sel, flags);
    idt_set_gate(44, (uint64_t)(uintptr_t)irq12, sel, flags);
    idt_set_gate(45, (uint64_t)(uintptr_t)irq13, sel, flags);
    idt_set_gate(46, (uint64_t)(uintptr_t)irq14, sel, flags);
    idt_set_gate(47, (uint64_t)(uintptr_t)irq15, sel, flags);

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}
