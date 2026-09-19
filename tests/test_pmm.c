#include <aura/pmm.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

/* Allocate 64MB virtual simulated RAM for unit test */
#define TEST_RAM_SIZE (64 * 1024 * 1024)

int main(void) {
    printf("Running PMM Unit Tests (Simulated 64MB RAM)...\n");
    
    uint8_t *simulated_ram = malloc(TEST_RAM_SIZE);
    assert(simulated_ram != NULL);
    
    /* Place bitmap at start of simulated RAM */
    uintptr_t bitmap_base = (uintptr_t)simulated_ram;
    pmm_init(TEST_RAM_SIZE, bitmap_base);
    
    assert(pmm_get_total_memory() == TEST_RAM_SIZE);
    assert(pmm_get_used_memory() == 0);
    assert(pmm_get_free_memory() == TEST_RAM_SIZE);
    
    /* Test single frame alloc */
    uintptr_t f1 = pmm_alloc_frame();
    assert(f1 == 0); /* First frame is 0 */
    assert(pmm_get_used_memory() == PAGE_SIZE);
    
    uintptr_t f2 = pmm_alloc_frame();
    assert(f2 == PAGE_SIZE);
    assert(pmm_get_used_memory() == 2 * PAGE_SIZE);
    
    /* Free f1 and realloc */
    pmm_free_frame(f1);
    assert(pmm_get_used_memory() == PAGE_SIZE);
    uintptr_t f3 = pmm_alloc_frame();
    assert(f3 == 0); /* Should reuse frame 0 */
    
    /* Test contiguous allocation (e.g., 16 frames = 64KB for DMA) */
    uintptr_t block = pmm_alloc_contiguous(16);
    assert(block % PAGE_SIZE == 0);
    assert(block >= 2 * PAGE_SIZE);
    
    printf("PMM Tests Passed: All allocations, frees, and contiguous blocks verified.\n");
    free(simulated_ram);
    return 0;
}
