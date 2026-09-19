#include <aura/gdt.h>
#include <stdint.h>
#include <stddef.h>

/* GDT with 5 standard entries (null, kcode, kdata, ucode, udata) + 1 16-byte TSS entry in 64-bit */
#if defined(__x86_64__)
struct gdt_table {
    struct gdt_entry entries[5];
    struct tss_entry_64 tss;
} __attribute__((packed));

static struct gdt_table gdt;
static struct tss_struct tss;
#else
static struct gdt_entry gdt[6];
#endif

static struct gdt_ptr gdt_p;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
#if defined(__x86_64__)
    gdt.entries[num].base_low = (uint16_t)(base & 0xFFFF);
    gdt.entries[num].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt.entries[num].base_high = (uint8_t)((base >> 24) & 0xFF);

    gdt.entries[num].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt.entries[num].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt.entries[num].granularity |= (gran & 0xF0);
    gdt.entries[num].access = access;
#else
    gdt[num].base_low = (uint16_t)(base & 0xFFFF);
    gdt[num].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[num].base_high = (uint8_t)((base >> 24) & 0xFF);

    gdt[num].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt[num].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
#endif
}

#if defined(__x86_64__)
static void write_tss(struct tss_entry_64 *g, uint64_t base, uint32_t limit)
{
    g->length = (uint16_t)(limit & 0xFFFF);
    g->base_low = (uint16_t)(base & 0xFFFF);
    g->base_mid = (uint8_t)((base >> 16) & 0xFF);
    g->flags1 = 0x89; /* Present, Ring 0, TSS (Available 64-bit TSS) */
    g->flags2 = (uint8_t)((limit >> 16) & 0x0F);
    g->base_high = (uint8_t)((base >> 24) & 0xFF);
    g->base_upper32 = (uint32_t)(base >> 32);
    g->reserved = 0;
}
#endif

void gdt_init(void)
{
    gdt_p.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_p.base = (uint64_t)(uintptr_t)&gdt;

    /* 0x00: Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);

#if defined(__x86_64__)
    /* 0x08: Kernel code: Present, Ring 0, Executable, Readable, Long mode (L=1) */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0x20);

    /* 0x10: Kernel data: Present, Ring 0, Writable */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0x00);

    /* 0x18: User code: Present, Ring 3, Executable, Readable, Long mode (L=1) */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0x20);

    /* 0x20: User data: Present, Ring 3, Writable */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0x00);

    /* Setup TSS */
    uint8_t *tss_ptr = (uint8_t *)&tss;
    for (size_t i = 0; i < sizeof(struct tss_struct); i++) {
        tss_ptr[i] = 0;
    }
    tss.iomap_base = sizeof(struct tss_struct);
    write_tss(&gdt.tss, (uint64_t)(uintptr_t)&tss, sizeof(struct tss_struct) - 1);

    /* Load GDT and reload segments */
    __asm__ volatile (
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "pushq $0x08\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "mov $0x28, %%ax\n\t"
        "ltr %%ax\n\t"
        :
        : "m"(gdt_p)
        : "rax", "memory"
    );
#else
    /* 32-bit protected mode */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    __asm__ volatile (
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:\n\t"
        :
        : "m"(gdt_p)
        : "eax", "memory"
    );
#endif
}
