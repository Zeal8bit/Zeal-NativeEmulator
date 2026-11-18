/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Look and feel: flat dark surfaces with retro phosphor accents, the data
 * font, panel headers and the small vector icons of the debugger. */
#pragma once

#include <FL/Enumerations.H>

namespace zui::theme {

constexpr Fl_Color COLOR_BG       = 0x111215'00;  /* window / gaps */
constexpr Fl_Color COLOR_SURFACE  = 0x181A1F'00;  /* panel body */
constexpr Fl_Color COLOR_RAISED   = 0x20232A'00;  /* headers, bars */
constexpr Fl_Color COLOR_BORDER   = 0x2E323B'00;
constexpr Fl_Color COLOR_HOVER    = 0x262A33'00;
constexpr Fl_Color COLOR_TEXT     = 0xD7DAE0'00;
constexpr Fl_Color COLOR_DIM      = 0x5E6575'00;
constexpr Fl_Color COLOR_AMBER    = 0xFFB454'00;  /* headers, labels, PC */
constexpr Fl_Color COLOR_CYAN     = 0x5CCFE6'00;  /* addresses, links */
constexpr Fl_Color COLOR_GREEN    = 0x95E06C'00;  /* running, lit flags */
constexpr Fl_Color COLOR_PC       = 0x2A3320'00;  /* current instruction row */
constexpr Fl_Color COLOR_SELECT   = 0x1F3A57'00;
constexpr Fl_Color COLOR_BREAK    = 0xFF5F57'00;
constexpr Fl_Color COLOR_CHANGED  = 0xFF8FD0'00;
/* Older names kept for the main window */
constexpr Fl_Color COLOR_PANEL    = COLOR_RAISED;
constexpr Fl_Color COLOR_ACCENT   = COLOR_CYAN;
constexpr Fl_Color COLOR_LABEL    = COLOR_AMBER;

constexpr int HEADER_H = 24;

/** Font for data views: BigBlue Terminal (8x16 pixel font) when available. */
extern Fl_Font DATA_FONT;
extern int DATA_SIZE;
extern bool g_pixel_font;

constexpr int BAR_H = 30; /* input/button rows on top of panels */

/** Fl::scheme and base colours of the whole UI. */
void apply_theme();
int  data_line_height();
/** Panel header: raised strip, amber accent bar, title and optional right text. */
void draw_header(int x, int y, int w, const char* title, const char* extra = nullptr);

enum class Icon { Play, Pause, StepInto, StepOver, RunTo, Reset, Breakpoint };
void draw_icon(Icon icon, int x, int y, int size, Fl_Color color);

} // namespace zui::theme
