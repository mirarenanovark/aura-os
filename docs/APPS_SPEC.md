# Project Aeris — Master Application Suite & GUI Specification

## 1. Unified Lightweight GUI Architecture (`libaeris-ui` / `libaglass`)
All core system applications use a shared, ultra-low-overhead C UI library:
* **Zero-copy retained-mode tree:** Minimal widget footprint (<4KB RAM per complex window).
* **Direct 9-slice + Dual Kawase Glass blending:** Every widget respects the global Aero Glass theme (semi-transparent backgrounds, specular borders, smooth rounded corners).
* **Microsecond response times:** Instant startup (<5ms) and zero UI frame drops.

---

## 2. Core First-Party Application Suite

### A. Productivity & Text
1. **Aero Notepad (`notepad`):**
   * Ultra-light text editor with UTF-8 support, line numbers, word wrap, and syntax highlighting hooks.
   * Instant cold start (<2ms), consumes <512KB RAM.
2. **Aero Write (`write` - Word Processor):**
   * Rich-text document editor supporting bold/italic/underline, font sizing, headings, alignments, image embeds, and `.rtf` / markdown export.
   * WYSIWYG page layout view with glass-accented ribbon/toolbar.
3. **Aero Calc (`calc` - Excel / Spreadsheet):**
   * High-performance grid table with formula evaluation (`SUM`, `AVG`, math expressions), cell referencing (`A1 + B2`), column auto-resizing, and CSV / TSV import/export.
   * Virtualized grid rendering: can scroll 100,000 rows at 60 FPS while keeping memory pinned to <2MB.

### B. System Diagnostics & Performance
4. **Task Manager (`taskmgr`):**
   * Real-time CPU, RAM, and GPU usage sparkline graphs.
   * Process tree view (PID, name, memory pages, thread count, priority, kill/suspend controls).
   * Services and loaded drivers inspection tab.
5. **BTOP-Style System Monitor (`btop-ui` / `btop`):**
   * Visual terminal/GUI hybrid dashboard with detailed per-core frequency, memory breakdown (kernel, cache, userland), storage I/O meters, and network bandwidth graphs.

### C. Developer & System Tools
6. **Terminal Emulator (`terminal`):**
   * ANSI 256-color & truecolor escape parser, scrollback buffer, TrueType font rendering, and transparency support.
7. **AI Coding Studio (`studio`):**
   * Lightweight IDE featuring code editor, MicroPython / TCC build buttons, live terminal pane, and an AI copilot sidebar for generating, reviewing, and testing OS code in real-time.
