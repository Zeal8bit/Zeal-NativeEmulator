/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Look and feel: flat surfaces with retro phosphor accents, the data
 * font, panel headers and the small vector icons of the debugger. */
#pragma once

#include <FL/Enumerations.H>

namespace zui::theme {

constexpr Fl_Color COLOR_BG       = FL_FREE_COLOR + 0;  /* window / gaps */
constexpr Fl_Color COLOR_SURFACE  = FL_FREE_COLOR + 1;  /* panel body */
constexpr Fl_Color COLOR_RAISED   = FL_FREE_COLOR + 2;  /* headers, bars */
constexpr Fl_Color COLOR_BORDER   = FL_FREE_COLOR + 3;
constexpr Fl_Color COLOR_HOVER    = FL_FREE_COLOR + 4;
constexpr Fl_Color COLOR_TEXT     = FL_FREE_COLOR + 5;
constexpr Fl_Color COLOR_DIM      = FL_FREE_COLOR + 6;
constexpr Fl_Color COLOR_AMBER    = FL_FREE_COLOR + 7;  /* headers, labels, PC */
constexpr Fl_Color COLOR_CYAN     = FL_FREE_COLOR + 8;  /* addresses, links */
constexpr Fl_Color COLOR_GREEN    = FL_FREE_COLOR + 9;  /* running, lit flags */
constexpr Fl_Color COLOR_PC       = FL_FREE_COLOR + 10;  /* current instruction row */
constexpr Fl_Color COLOR_SELECT   = FL_FREE_COLOR + 11;
constexpr Fl_Color COLOR_BREAK    = FL_FREE_COLOR + 12;
constexpr Fl_Color COLOR_CHANGED  = FL_FREE_COLOR + 13;
constexpr Fl_Color COLOR_MATCH    = FL_FREE_COLOR + 14;  /* memory search hits */
constexpr Fl_Color COLOR_BREAK_ROW = FL_FREE_COLOR + 15; /* watch that stopped the CPU */
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
void start_theme();
void apply_theme_preference();
void stop_theme();
int  data_line_height();
/** Panel header: raised strip, amber accent bar, title and optional right text. */
void draw_header(int x, int y, int w, const char* title, const char* extra = nullptr);

enum class Icon { Play, Pause, StepInto, StepOver, RunTo, Reset, Breakpoint };
void draw_icon(Icon icon, int x, int y, int size, Fl_Color color);

} // namespace zui::theme
