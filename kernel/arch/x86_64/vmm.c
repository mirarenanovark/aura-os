#include <aura/vmm.h>
#include <aura/pmm.h>

#define PML4_INDEX(virt) (((virt) >> 39) & 0x1FFULL)
#define PDPT_INDEX(virt) (((virt) >> 30) & 0x1FFULL)
#define PD_INDEX(virt)   (((virt) >> 21) & 0x1FFULL)
#define PT_INDEX(virt)   (((virt) >> 12) & 0x1FFULL)

#define PTE_ADDR_MASK    0x000FFFFFFFFFF000ULL

static inline void invlpg(uintptr_t addr) {
    __asm__ __volatile__("invlpg (%0)" : : "r"(addr) : "memory");
}

static inline void write_cr3(uint64_t val) {
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(val) : "memory");
}

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(val));
    return val;
}

void vmm_init(void) {
    uint64_t *pml4 = (uint64_t *)(uintptr_t)read_cr3();
    vmm_switch_pml4(pml4);
}

void vmm_map_page(uint64_t *pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    uint64_t *pdpt, *pd, *pt;

    /* PML4 -> PDPT */
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        pdpt = (uint64_t *)(uintptr_t)pmm_alloc_frame();
        for (int i = 0; i < 512; i++) pdpt[i] = 0;
        pml4[pml4_idx] = (uint64_t)(uintptr_t)pdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else {
        pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & PTE_ADDR_MASK);
    }

    /* PDPT -> PD */
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        pd = (uint64_t *)(uintptr_t)pmm_alloc_frame();
        for (int i = 0; i < 512; i++) pd[i] = 0;
        pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else {
        pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & PTE_ADDR_MASK);
    }

    /* PD -> PT */
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        pt = (uint64_t *)(uintptr_t)pmm_alloc_frame();
        for (int i = 0; i < 512; i++) pt[i] = 0;
        pd[pd_idx] = (uint64_t)(uintptr_t)pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else {
        pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & PTE_ADDR_MASK);
    }

    /* PT -> Page */
    pt[pt_idx] = (phys & PTE_ADDR_MASK) | (flags & ~PTE_ADDR_MASK) | PTE_PRESENT;
    vmm_invlpg(virt);
}

void vmm_unmap_page(uint64_t *pml4, uintptr_t virt) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    if (!(pml4[pml4_idx] & PTE_PRESENT)) return;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return;
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT)) return;
    uint64_t *pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & PTE_ADDR_MASK);

    if (!(pt[pt_idx] & PTE_PRESENT)) return;

    pt[pt_idx] = 0;
    vmm_invlpg(virt);
}

uintptr_t vmm_virt_to_phys(uint64_t *pml4, uintptr_t virt) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return 0;
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT)) return 0;
    uint64_t *pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & PTE_ADDR_MASK);

    if (!(pt[pt_idx] & PTE_PRESENT)) return 0;

    return (uintptr_t)((pt[pt_idx] & PTE_ADDR_MASK) | (virt & 0xFFFULL));
}

void vmm_switch_pml4(uint64_t *pml4) {
    write_cr3((uint64_t)(uintptr_t)pml4);
}

void vmm_invlpg(uintptr_t virt) {
    invlpg(virt);
}
