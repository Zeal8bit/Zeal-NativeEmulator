/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "theme/theme.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

namespace zui::theme {

void apply_theme()
{
    Fl::scheme("gtk+");
    Fl::background(0x1a, 0x1c, 0x21);
    Fl::background2(0x12, 0x13, 0x16);
    Fl::foreground(0xd7, 0xda, 0xe0);
    Fl::set_color(FL_SELECTION_COLOR, 0x2c, 0x5a, 0x8a);
    Fl::set_color(FL_INACTIVE_COLOR, 0x55, 0x5a, 0x66);
}

int data_line_height()
{
    fl_font(DATA_FONT, DATA_SIZE);
    return fl_height() + (g_pixel_font ? 2 : 4);
}

void draw_header(int x, int y, int w, const char* title, const char* extra)
{
    fl_color(COLOR_RAISED);
    fl_rectf(x, y, w, HEADER_H);
    fl_color(COLOR_BORDER);
    fl_xyline(x, y + HEADER_H - 1, x + w - 1);
    fl_color(COLOR_AMBER);
    fl_rectf(x, y, 3, HEADER_H - 1);
    fl_font(DATA_FONT, DATA_SIZE);
    fl_color(COLOR_AMBER);
    fl_draw(title, x + 10, y + HEADER_H - 1 - (HEADER_H - fl_height()) / 2 - fl_descent());
    if (extra && *extra) {
        fl_color(COLOR_DIM);
        fl_draw(extra, x + w - (int) fl_width(extra) - 8,
                y + HEADER_H - 1 - (HEADER_H - fl_height()) / 2 - fl_descent());
    }
}

void draw_icon(Icon icon, int x, int y, int s, Fl_Color c)
{
    fl_color(c);
    const int cx = x + s / 2;
    const int cy = y + s / 2;
    const int r = s / 3;
    switch (icon) {
        case Icon::Play:
            fl_polygon(cx - r + 1, cy - r, cx + r, cy, cx - r + 1, cy + r);
            break;
        case Icon::Pause:
            fl_rectf(cx - r, cy - r, r * 2 / 3 + 1, r * 2 + 1);
            fl_rectf(cx + r / 3, cy - r, r * 2 / 3 + 1, r * 2 + 1);
            break;
        case Icon::StepInto: /* arrow down onto a bar */
            fl_rectf(cx - 1, cy - r, 3, r);
            fl_polygon(cx - r + 1, cy, cx + r - 1, cy, cx, cy + r - 2);
            fl_rectf(cx - r, cy + r, r * 2 + 1, 2);
            break;
        case Icon::StepOver: /* arc over a bar */
            fl_line_style(FL_SOLID, 2);
            fl_arc(cx - r, cy - r, r * 2, r * 2, 0, 180);
            fl_line_style(0);
            fl_polygon(cx + r - 4, cy, cx + r + 4, cy, cx + r, cy + 5);
            fl_rectf(cx - r, cy + r, r * 2 + 1, 2);
            break;
        case Icon::RunTo: /* play into a bar */
            fl_polygon(cx - r, cy - r, cx + r / 2, cy, cx - r, cy + r);
            fl_rectf(cx + r / 2 + 2, cy - r, 2, r * 2 + 1);
            break;
        case Icon::Reset: /* circular arrow */
            fl_line_style(FL_SOLID, 2);
            fl_arc(cx - r, cy - r, r * 2, r * 2, 60, 360);
            fl_line_style(0);
            fl_polygon(cx + r - 4, cy - r - 3, cx + r + 4, cy - r + 1, cx + r - 2, cy - r + 6);
            break;
        case Icon::Breakpoint:
            fl_pie(cx - r, cy - r, r * 2, r * 2, 0, 360);
            break;
    }
}

} // namespace zui::theme
