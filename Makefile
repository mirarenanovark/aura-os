CC = x86_64-linux-gnu-gcc
LD = x86_64-linux-gnu-ld
HOST_CC = gcc

CFLAGS = -ffreestanding -nostdlib -nostartfiles -m64 -mcmodel=kernel \
         -fno-pie -fno-stack-protector -mno-red-zone -Wall -Wextra \
         -Ikernel/include -O2

ASFLAGS = -m64 -ffreestanding

BUILD_DIR = build
ISO_DIR = $(BUILD_DIR)/isodir

C_SRCS = kernel/main.c \
         kernel/core/multiboot2.c \
         kernel/core/pmm.c \
         kernel/core/heap.c \
         kernel/core/zram.c \
         kernel/core/panic.c \
         kernel/core/sysmon.c \
         kernel/core/dashboard.c \
         kernel/core/menu.c \
         kernel/drivers/vga.c \
         kernel/drivers/tui.c \
         kernel/drivers/keyboard.c \
         kernel/arch/x86_shared/serial.c \
         kernel/arch/x86_shared/gdt.c \
         kernel/arch/x86_shared/idt.c \
         kernel/arch/x86_shared/pit.c \
         kernel/arch/x86_64/vmm.c

ASM_SRCS = boot/multiboot2_header.S \
           boot/entry64.S \
           kernel/arch/x86_shared/isr.S

C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst %.S, $(BUILD_DIR)/%.o, $(ASM_SRCS))

OBJS = $(ASM_OBJS) $(C_OBJS)

all: $(BUILD_DIR)/auraos.elf $(BUILD_DIR)/auraos.iso

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/auraos.elf: $(OBJS) kernel/linker.ld
	@mkdir -p $(BUILD_DIR)
	$(LD) -T kernel/linker.ld -nostdlib $(OBJS) -o $@

$(BUILD_DIR)/auraos.iso: $(BUILD_DIR)/auraos.elf boot/iso/boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(BUILD_DIR)/auraos.elf $(ISO_DIR)/boot/auraos.elf
	@cp boot/iso/boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/auraos.iso $(ISO_DIR)

run: $(BUILD_DIR)/auraos.iso
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/auraos.iso -serial stdio -display none -no-reboot

test: test-pmm test-heap test-zram

test-pmm: tests/test_pmm.c kernel/core/pmm.c
	@mkdir -p $(BUILD_DIR)
	$(HOST_CC) -Ikernel/include tests/test_pmm.c kernel/core/pmm.c -o $(BUILD_DIR)/test_pmm
	@./$(BUILD_DIR)/test_pmm

test-heap: tests/test_heap.c kernel/core/heap.c kernel/core/pmm.c
	@mkdir -p $(BUILD_DIR)
	$(HOST_CC) -Ikernel/include tests/test_heap.c kernel/core/heap.c kernel/core/pmm.c -o $(BUILD_DIR)/test_heap
	@./$(BUILD_DIR)/test_heap

test-zram: tests/test_zram.c kernel/core/zram.c kernel/core/heap.c kernel/core/pmm.c
	@mkdir -p $(BUILD_DIR)
	$(HOST_CC) -Ikernel/include tests/test_zram.c kernel/core/zram.c kernel/core/heap.c kernel/core/pmm.c -o $(BUILD_DIR)/test_zram
	@./$(BUILD_DIR)/test_zram

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run test test-pmm test-heap test-zram clean
