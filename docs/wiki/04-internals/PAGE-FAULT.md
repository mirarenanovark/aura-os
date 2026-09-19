# Page Fault Handler (`#PF`, Vector 14)

## Overview

When the CPU tries to access virtual memory that isn't mapped, or violates memory permissions, the Memory Management Unit (MMU) triggers Exception 14: a **Page Fault** (`#PF`).

AuraOS catches this exception, captures the faulting address from the **CR2** register, decodes the hardware error code into plain English, and drops the user into the interactive panic screen where they can choose to **Restore** the system, **Reboot**, or **Power Off**.

---

## 1. Boot Registration

During kernel initialization in `kernel/main.c`, right after the IDT is installed:

```c
pagefault_init();
```

This calls `isr_register_handler(14, page_fault_handler)` in `kernel/arch/x86_shared/idt.c`. When vector 14 fires, `isr_handler()` routes execution directly to our C handler instead of the generic CPU exception path.

---

## 2. Handler Execution Flow

```
CPU Page Fault (#PF, Vector 14)
    │
    ▼
isr14 stub (boot/isr.S)
    │ Pushes dummy/hardware error code to stack
    │ Saves all 15 general-purpose registers into registers_t
    ▼
isr_handler(regs) (kernel/arch/x86_shared/idt.c)
    │ Vector 14 has a registered handler? Yes.
    ▼
page_fault_handler(regs) (kernel/core/pagefault.c)
    │
    ├─ 1. Read CR2 first
    │     asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    │     (Must read before any memory access could trigger another fault)
    │
    ├─ 2. Decode error code bits
    │     bit 0: Present?    (0 = page not present, 1 = permission denied)
    │     bit 1: Write?      (0 = read attempt,     1 = write attempt)
    │     bit 2: User mode?  (0 = kernel/ring 0,    1 = user/ring 3)
    │     bit 3: Reserved?   (1 = reserved bit in page table was set)
    │     bit 4: Exec?       (1 = instruction fetch on NX page)
    │
    ├─ 3. Pick clear human description
    │     e.g., "page fault: kernel write to non-present page"
    │
    ├─ 4. Append the faulting address from CR2
    │     e.g., "... at 0x00000000dead0000"
    │
    └─ 5. aura_panic_interactive(msg, file, line, regs)
          Displays registers, fault reason, and waits for user key:
          [R] Restore execution from last checkpoint
          [B] Cold reboot
          [O] Power off via ACPI/QEMU shutdown
```

---

## 3. Hardware Error Code Bit Table

The CPU pushes an error code onto the stack before invoking vector 14. Here is what each bit means under the x86 architecture standard:

| Bit | Name | When `0` | When `1` |
|:---|:---|:---|:---|
| **0** | `Present` | Page does not exist in page tables (unmapped) | Page exists, but access was forbidden (protection violation) |
| **1** | `Write` | Access was a **read** | Access was a **write** |
| **2** | `User` | Access came from **kernel mode** (Ring 0) | Access came from **user mode** (Ring 3) |
| **3** | `Reserved` | Normal page table entry | A reserved bit in a page table entry was set to 1 |
| **4** | `Instruction` | Data read or write | Instruction fetch from a page marked No-Execute (NX) |

---

## 4. Why CR2 Must Be Read First

The **CR2** control register holds the linear (virtual) memory address that caused the fault. 

The CPU updates CR2 on every page fault. If the handler makes a bad pointer access before reading CR2, a second page fault would overwrite CR2, destroying the original fault address forever. Reading CR2 as the very first assembly statement guarantees the original faulting address is preserved.

---

## 5. Testing the Handler Live

AuraOS includes a built-in test trigger in the boot menu:

1. Boot AuraOS in QEMU or on real hardware.
2. At the `aura> ` prompt, type:
   ```text
   aura> pf
   ```
3. `trigger_pf()` in `kernel/core/menu.c` executes:
   ```c
   volatile uint64_t *p = (volatile uint64_t *)0xdead0000;
   *p = 0xcafebabe;
   ```
4. Address `0xdead0000` is unmapped memory. The CPU raises vector 14.
5. You will see the panic screen appear:
   * **Reason:** `page fault: kernel write to non-present page at 0x00000000dead0000`
   * **Registers:** RIP pointing into `trigger_pf`, RSP, CR2 address.
   * **Options:** Press **R** to restore cleanly back to the boot menu shell!

---

## 6. Source Files

* `kernel/core/pagefault.c` — CR2 read, bit decoding, and panic trigger.
* `kernel/include/aura/pagefault.h` — `pagefault_init()` public prototype.
* `kernel/arch/x86_shared/idt.c` — Vector 14 registration via `isr_register_handler()`.
* `kernel/core/menu.c` — `pf` test command.
