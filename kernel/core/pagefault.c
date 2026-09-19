/**
 * @file        kernel/core/pagefault.c
 * @layer       STAGE_2_PHYSICAL_FAULT
 * @component   PAGE_FAULT_HANDLER
 * @contract    PRD-02-capabilities, PRD-06-verification
 * @description x86_64 Page Fault (#PF, vector 14) exception handler.
 *              Reads CR2, decodes error code bits, builds human-readable
 *              reason string, and calls aura_panic_interactive() for
 *              interactive [R]estore/[B]oot/[O]ff recovery.
 *
 * @connects
 *              - Upstream:   kernel/main.c (pagefault_init)
 *              - Downstream: kernel/arch/x86_shared/idt.c (isr_register_handler)
 *              - Hardware:   CPU CR2 register (faulting address)
 *
 * @flow        [AURA_FLOW: PAGE_FAULT_HANDLER]
 *              1. Read CR2 (faulting address) into local variable.
 *              2. Decode error code bits (Present, Write, User, Reserved, Instr-fetch).
 *              3. Build human-readable reason string.
 *              4. Call aura_panic_interactive() for interactive recovery.
 */

#include <aura/pagefault.h>
#include <aura/idt.h>
#include <aura/panic.h>
#include <stdint.h>

/* Error code bit definitions (x86 architecture standard) */
#define PF_PRESENT      (1U << 0)   /* 0 = non-present page; 1 = protection fault */
#define PF_WRITE        (1U << 1)   /* 0 = read; 1 = write */
#define PF_USER         (1U << 2)   /* 0 = supervisor (ring 0); 1 = user (ring 3) */
#define PF_RESERVED     (1U << 3)   /* 1 = reserved bit set in page directory/table */
#define PF_INSTR_FETCH  (1U << 4)   /* 1 = instruction fetch on NX page */

/* Helper to convert uint64 to 16-hex-digit string into buf (must be at least 19 bytes) */
static void hex_to_str(uint64_t v, char *out) {
    static const char hex[] = "0123456789abcdef";
    out[0] = '0';
    out[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        out[2 + (15 - i)] = hex[(v >> (i * 4)) & 0x0F];
    }
    out[18] = '\0';
}

static void page_fault_handler(registers_t *regs)
{
    /* Read CR2 FIRST — CPU overwrites it on any subsequent fault */
    uint64_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    /* Decode error code bits */
    uint32_t err = (uint32_t)regs->err_code;
    int present     = (err & PF_PRESENT) != 0;
    int write       = (err & PF_WRITE) != 0;
    int user        = (err & PF_USER) != 0;
    int reserved    = (err & PF_RESERVED) != 0;
    int instr_fetch = (err & PF_INSTR_FETCH) != 0;

    /* Base descriptions */
    const char *desc = "page fault";
    if (reserved) {
        desc = "page fault: reserved bit set in page tables";
    } else if (instr_fetch) {
        desc = present ? "page fault: exec on NX present page" : "page fault: exec on non-present page";
    } else if (user) {
        if (write) {
            desc = present ? "page fault: user write protection violation" : "page fault: user write to non-present page";
        } else {
            desc = present ? "page fault: user read protection violation" : "page fault: user read from non-present page";
        }
    } else {
        if (write) {
            desc = present ? "page fault: kernel write protection violation" : "page fault: kernel write to non-present page";
        } else {
            desc = present ? "page fault: kernel read protection violation" : "page fault: kernel read from non-present page";
        }
    }

    /* Build message showing both reason and CR2 address */
    static char msg[128];
    char hex_addr[20];
    hex_to_str(fault_addr, hex_addr);

    /* Construct: "<desc> at <0x...>" */
    int i = 0;
    while (desc[i] && i < 80) {
        msg[i] = desc[i];
        i++;
    }
    const char *at = " at ";
    int j = 0;
    while (at[j]) msg[i++] = at[j++];
    j = 0;
    while (hex_addr[j]) msg[i++] = hex_addr[j++];
    msg[i] = '\0';

    /* Trigger interactive kernel panic: [R]estore / [B]oot / [O]ff */
    aura_panic_interactive(msg, __FILE__, __LINE__, regs);
}

void pagefault_init(void)
{
    /* Register page fault handler for vector 14 */
    isr_register_handler(14, page_fault_handler);
}
