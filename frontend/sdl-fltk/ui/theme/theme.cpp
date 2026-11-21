/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "theme/theme.h"
#include "common/machine.h"
#include "theme/system_appearance.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

namespace zui::theme {

namespace {
struct ThemeColor {
    Fl_Color slot;
    unsigned dark;
    unsigned light;
};

constexpr ThemeColor colors[] = {
    {COLOR_BG,      0x111215'00, 0xEFF2F5'00},
    {COLOR_SURFACE, 0x181A1F'00, 0xFFFFFF'00},
    {COLOR_RAISED,  0x20232A'00, 0xE8EDF1'00},
    {COLOR_BORDER,  0x2E323B'00, 0xA4B2BF'00},
    {COLOR_HOVER,   0x262A33'00, 0xE1E8EF'00},
    {COLOR_TEXT,    0xD7DAE0'00, 0x17232F'00},
    {COLOR_DIM,     0x5E6575'00, 0x4F5C69'00},
    {COLOR_AMBER,   0xFFB454'00, 0x794300'00},
    {COLOR_CYAN,    0x5CCFE6'00, 0x00546B'00},
    {COLOR_GREEN,   0x95E06C'00, 0x285B23'00},
    {COLOR_PC,      0x2A3320'00, 0xE3F1D9'00},
    {COLOR_SELECT,  0x1F3A57'00, 0xC7DDF3'00},
    {COLOR_BREAK,   0xFF5F57'00, 0xB3261E'00},
    {COLOR_CHANGED, 0xFF8FD0'00, 0x822B63'00},
    {COLOR_MATCH,   0x3D3320'00, 0xF3DDA6'00},
    {COLOR_BREAK_ROW, 0x5D2E2F'00, 0xF0D4D2'00},
    {FL_SELECTION_COLOR, 0x2C5A8A'00, 0xC7DDF3'00},
    {FL_INACTIVE_COLOR,  0x555A66'00, 0x596673'00},
};

bool using_light;

void apply_palette(bool light)
{
    using_light = light;
    if (light) {
        Fl::background(0xe8, 0xed, 0xf1);
        Fl::background2(0xff, 0xff, 0xff);
        Fl::foreground(0x17, 0x23, 0x2f);
    } else {
        Fl::background(0x1a, 0x1c, 0x21);
        Fl::background2(0x12, 0x13, 0x16);
        Fl::foreground(0xd7, 0xda, 0xe0);
    }
    for (const auto& color : colors)
        Fl::set_color(color.slot, light ? color.light : color.dark);
}

void check_appearance(void*)
{
    const bool light = system_uses_light_theme();
    if (light != using_light) {
        apply_palette(light);
        Fl::redraw();
    }
    Fl::repeat_timeout(1.0, check_appearance);
}
} // namespace

void start_theme()
{
    Fl::scheme("gtk+");
    apply_theme_preference();
}

void apply_theme_preference()
{
    Fl::remove_timeout(check_appearance);
    if (config.window.theme == THEME_SYSTEM) {
        apply_palette(system_uses_light_theme());
        Fl::add_timeout(1.0, check_appearance);
    } else {
        close_system_appearance();
        apply_palette(config.window.theme == THEME_LIGHT);
    }
    Fl::redraw();
}

void stop_theme()
{
    Fl::remove_timeout(check_appearance);
    close_system_appearance();
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
