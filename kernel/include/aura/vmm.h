/**
 * @file        kernel/include/aura/vmm.h
 * @layer       STAGE_3_VIRTUAL_SCHEDULER
 * @component   VMM_4LEVEL_PAGING
 * @contract    PRD-02-capabilities-protection
 * @description Virtual Memory Manager (VMM) contract for 4-level x86_64 paging.
 *              Page table entry flags, mapping, unmapping, virtual translation,
 *              CR3 address space switching, and TLB page invalidation.
 *
 * @connects
 *              - Upstream:   kernel/main.c, future userland process loaders
 *              - Downstream: kernel/arch/x86_64/vmm.c, kernel/core/pmm.c
 *              - Hardware:   CPU CR3 register, MMU page table walker, TLB
 */

#ifndef AURA_VMM_H
#define AURA_VMM_H

#include <stdint.h>
#include <stddef.h>

/* Page Table Entry Flags (x86_64) */
#define PTE_PRESENT       (1ULL << 0)
#define PTE_WRITABLE      (1ULL << 1)
#define PTE_USER          (1ULL << 2)
#define PTE_WRITE_THROUGH (1ULL << 3)
#define PTE_CACHE_DISABLE (1ULL << 4)
#define PTE_PAT_WC        (1ULL << 7)
#define PTE_NX            (1ULL << 63)

void      vmm_init(void);
void      vmm_map_page(uint64_t *pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void      vmm_unmap_page(uint64_t *pml4, uintptr_t virt);
uintptr_t vmm_virt_to_phys(uint64_t *pml4, uintptr_t virt);
void      vmm_switch_pml4(uint64_t *pml4);
void      vmm_invlpg(uintptr_t virt);

#endif /* AURA_VMM_H */
