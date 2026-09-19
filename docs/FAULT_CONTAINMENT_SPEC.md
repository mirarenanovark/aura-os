# AuraOS Crash Resilience & Fault-Containment Architecture

**Consultant:** Astra (`gpt-6-astra`)  
**Topic:** Modular fault containment, kernel panic survival, warm restart vs. hardware reset.  
**Repository:** https://github.com/mirarenanovark/aura-os  
**Status:** Approved & Formally Specified

---

## 1. Executive Verdict
> **"Use a small kernel, isolate failure-prone services where practical, and add a minimal panic-to-reboot path plus hardware watchdog; do not build a hypervisor or promise recovery from arbitrary kernel corruption."**

### Core Philosophy Alignment
Replace the marketing slogan *"the PC never has to die"* with the real engineering law:
**"Contain ordinary failures, restart failed services instantly, preserve committed storage, and provide a clean <500ms warm reset path when the kernel panics."**

---

## 2. The 3-Tier Fault Containment Model

```
┌─────────────────────────────────────────────────────────────┐
│ Tier 1: Userland Applications & Capsules                    │
│ • Notepad, Games, APK Capsules, Calc                        │
│ • Crash behavior: Terminated by kernel. System 100% stable. │
└──────────────────────────────┬──────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────┐
│ Tier 2: Isolated Userland Services + Supervisor Tree        │
│ • Window Server, Audio Mixer, Network Stack, Filesystem     │
│ • Supervised by `aura-init` (Erlang-style simple watchdog)   │
│ • Crash behavior: Service restarts instantly; client apps    │
│   reconnect or reconnect their shared-memory surfaces.      │
│   KERNEL NEVER DIES.                                        │
└──────────────────────────────┬──────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────┐
│ Tier 3: Core Monolithic Freestanding C Kernel               │
│ • Scheduler, Physical/Virtual Memory, IRQ Dispatch, Hal     │
│ • Emergency Stub: Dedicated IDT Double-Fault / IST stack    │
│ • On Panic: Writes clean diagnostic record, flushes pending │
│   storage transactions, and performs a clean warm kexec     │
│   or hardware platform reset in <500ms without cold reboot. │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. What CAN Be Recovered vs. What Requires Clean Reset

### ✅ 100% Recoverable Without Reboot:
* **Application crashes:** Faulting process terminated, page tables freed, zero impact on desktop.
* **Window Server crashes:** `aura-init` supervisor relaunches the Window Server in <50ms; apps reconnect to new shared-memory handles.
* **Audio driver / network service hang:** Service process killed and restarted.
* **Clean filesystem caches:** Discarded and reconstructed from disk.

### ⚠️ Requires Clean Warm Handoff / Reset:
* **Corrupted kernel page tables / corrupted kernel heap:** Never attempt to keep running a corrupted kernel in-place — doing so corrupts user data and storage.
* **Wild Ring-0 writes:** In a monolithic architecture on vintage x86 (P3/P4), all Ring-0 code shares address space.
* **Uncontrolled DMA devices:** Devices bus-mastering corrupt memory must be reset by hardware.

---

## 4. Implementation Details (Freestanding C + Asm)

1. **Emergency Stacks:**
   * **x86_64:** Use Interrupt Stack Table (IST) for Double Fault (#DF) and NMI, ensuring a clean stack even if the kernel stack overflows.
   * **i686:** Dedicated IDT task-gate TSS with a dedicated emergency stack and clean CR3.
   * **ARM:** Dedicated privileged EL1 / Abort mode exception stacks.
2. **Panic Handler Contract:**
   * 100% lock-free, allocation-free, non-sleeping C function.
   * Dumps CPU registers to serial port (`COM1` / `PL011`).
   * Writes a fixed-size 512-byte crash log to a reserved physical RAM page.
   * Triggers a clean platform warm reset or jump to reserved clean kernel image.
3. **Hardware Watchdog:**
   * Support standard platform watchdog timers (iTCO on Intel, BCM watchdog on Raspberry Pi) so deadlocks trigger automated reset instead of a frozen screen.
