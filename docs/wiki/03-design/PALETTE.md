# AuraOS UI Theme & Design Palette Spec

**Version:** 1.0 (v0.3.1) · **Designer:** Astra / Mira · **Status:** CANONICAL

## Overview
AuraOS enforces visual unity between the **freestanding kernel text user interface (VGA 80x25)** and the **AuraOS Web Release Portal**. 

---

## Canonical Palette Roles

| Role | Semantic Meaning | Web Hex | VGA Index | VGA Constant |
|:---|:---|:---:|:---:|:---|
| **`BG`** | Screen Background | `#0a0e14` | `0` | `VGA_COLOR_BLACK` |
| **`PANEL`** | Box/Window Interior | `#11161f` | `0` | `VGA_COLOR_BLACK` |
| **`BORDER`** | Box Lines, Tracks, Dividers | `#3b4453` | `8` | `VGA_COLOR_DARK_GREY` |
| **`PRIMARY`** | Titles, Links, Accent Highlights | `#00d7ff` | `11` | `VGA_COLOR_LIGHT_CYAN` |
| **`SUCCESS`** | Badges, OK, Bars, Prompt | `#5af78e` | `10` | `VGA_COLOR_LIGHT_GREEN` |
| **`WARN`** | Warnings, Cursors, Section Labels | `#f3f99d` | `14` | `VGA_COLOR_LIGHT_BROWN` (Yellow) |
| **`DANGER`** | Panics, Critical Faults | `#ff5555` | `12` | `VGA_COLOR_LIGHT_RED` |
| **`TEXT`** | Body Text, Value Labels | `#e6e6e6` | `15` | `VGA_COLOR_WHITE` |
| **`DIM`** | Table Footers, Inactive Text | `#8b949e` | `7` | `VGA_COLOR_LIGHT_GREY` |
| **`MAGENTA`** | Memory Gauges, Stage Tags | `#ff6ac1` | `13` | `VGA_COLOR_LIGHT_MAGENTA` |

---

## Implementation Reference

* **Kernel Header:** `kernel/include/aura/theme.h`
* **Web Portal CSS:** `var(--cyan)`, `var(--green)`, `var(--yellow)`, `var(--magenta)`, `var(--border)`, `var(--bg)` in `/home/xor/auraos-releases/portal.py`
