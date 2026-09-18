# Aeris System Monitor (`sysmon` / `taskmgr`) — Architecture & UI Contract

## 1. UX & Visual Design: Ubuntu-Style (GNOME System Monitor / Resources)
The task manager adopts Ubuntu's clean, intuitive multi-tab layout, but reimagined in **Aero Glass & Frost**:
* **Tab 1: Processes (`Processes`):**
  * Clean tree-list view with application icons and clear hierarchical grouping.
  * Columns: `Process Name`, `CPU %`, `Memory`, `Disk Read/Write`, `PID`, `Priority`.
  * Inline quick search / filter bar at top right.
  * Right-click context menu: `End Process`, `Kill`, `Pause / Resume`, `Set Priority (Nice)`, `Inspect Open Handles`.
* **Tab 2: Resources (`Resources`):**
  * Top section: **CPU History** (smooth colored line graphs for each CPU core + overall load).
  * Middle section: **Memory and Swap History** (gradient filled wave graphs showing real-time allocation).
  * Bottom section: **Network & Disk History** (inbound/outbound traffic and I/O throughput sparklines).
* **Tab 3: File Systems & Hardware (`Storage`):**
  * Visual progress bars for mounted disks, RAM disks, and partition usage.

---

## 2. Performance & Low-Overhead Engineering
Unlike modern Electron or heavy GTK/JS system monitors that take 100MB+ of RAM and 5% CPU just to monitor the system:
* **Footprint:** Built in freestanding C with `libaeris-ui`. Uses **< 1.2 MB RAM** total.
* **Polling Loop:** Event-driven with an internal timer (500ms or 1000ms user-adjustable interval).
* **Zero Allocations During Refresh:** Ring buffer of fixed-size history data points. Sampling uses a direct kernel syscall (`sys_proc_table`) rather than parsing thousands of text files in `/proc`.
* **Instant Cold Start:** Under 5ms launch time.
