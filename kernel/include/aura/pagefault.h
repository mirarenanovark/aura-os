/**
 * @file        kernel/include/aura/pagefault.h
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
 */

#ifndef AURA_PAGEFAULT_H
#define AURA_PAGEFAULT_H

#include <stdint.h>

/* Initialize page fault handler: registers vector 14 handler via isr_register_handler */
void pagefault_init(void);

#endif /* AURA_PAGEFAULT_H */
