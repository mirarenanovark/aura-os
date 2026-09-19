/**
 * @file        kernel/include/aura/theme.h
 * @layer       STAGE_4_SERVICES_DRIVERS
 * @component   THEME_PALETTE
 * @description Canonical AuraOS UI theme palette.
 *              Unified 1:1 mapping between web release portal hex CSS variables
 *              and VGA 16-color ANSI text attributes.
 *
 * Palette Rationale:
 * - Direct mapping to standard VGA DAC bright indices (10..15).
 * - Release Portal: --cyan (#00d7ff), --green (#5af78e), --yellow (#f3f99d), --text (#e6e6e6).
 * - Kernel TUI: Matches visual hierarchy (Cyan primary, Green success, Yellow warn, White text).
 */

#ifndef AURA_THEME_H
#define AURA_THEME_H

#include <aura/vga.h>

/* Semantic Color Roles (VGA 16-Color Values) */
#define COLOR_BG          VGA_COLOR_BLACK         /* 0  - Web: #0a0e14 */
#define COLOR_PANEL       VGA_COLOR_BLACK         /* 0  - Web: #11161f (black on VGA) */
#define COLOR_BORDER      VGA_COLOR_DARK_GREY     /* 8  - Web: #3b4453 */
#define COLOR_PRIMARY     VGA_COLOR_LIGHT_CYAN    /* 11 - Web: #00d7ff (Cyan) */
#define COLOR_SUCCESS     VGA_COLOR_LIGHT_GREEN   /* 10 - Web: #5af78e (Neon Green) */
#define COLOR_WARN        VGA_COLOR_LIGHT_BROWN   /* 14 - Web: #f3f99d (Yellow) */
#define COLOR_DANGER      VGA_COLOR_LIGHT_RED     /* 12 - Web: #ff5555 (Red) */
#define COLOR_TEXT        VGA_COLOR_WHITE         /* 15 - Web: #e6e6e6 (Bright White) */
#define COLOR_DIM         VGA_COLOR_LIGHT_GREY    /* 7  - Web: #8b949e (Dim Grey) */
#define COLOR_MAGENTA     VGA_COLOR_LIGHT_MAGENTA /* 13 - Web: #ff6ac1 (Stage / Badge) */

#endif /* AURA_THEME_H */
