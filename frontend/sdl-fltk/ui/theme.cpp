/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Look and feel: palette, the BigBlue Terminal pixel font for data views,
 * section headers and the small vector icons used by the debugger. */

#include <cstdio>
#include <cstring>
#include <FL/fl_draw.H>
#include "ui.h"

#if defined(__APPLE__)
#include <CoreText/CoreText.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(ZUI_HAVE_FONTCONFIG)
#include <fontconfig/fontconfig.h>
#endif

extern "C" {
#include "utils/paths.h"
}

#ifndef ZEAL_ASSETS_DIR
#define ZEAL_ASSETS_DIR "assets"
#endif

namespace zui {

Fl_Font DATA_FONT = FL_COURIER;
int DATA_SIZE = 13;
bool g_pixel_font = false;

static const char* FONT_FILE = "BigBlue_Terminal_437TT.TTF";
static const char* FONT_FAMILY = "BigBlue Terminal 437TT";

static bool register_font_file(const char* path)
{
#if defined(__APPLE__)
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*) path, (CFIndex) strlen(path), false);
    if (url == nullptr) return false;
    const bool ok = CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr);
    CFRelease(url);
    return ok;
#elif defined(_WIN32)
    return AddFontResourceExA(path, FR_PRIVATE, 0) > 0;
#elif defined(ZUI_HAVE_FONTCONFIG)
    return FcConfigAppFontAddFile(nullptr, (const FcChar8*) path) == FcTrue;
#else
    (void) path;
    return false;
#endif
}

void load_fonts()
{
    char exe_dir[PATH_MAX];
    char path[PATH_MAX + 64];
    get_executable_dir(exe_dir, sizeof(exe_dir));

    const char* candidates[3];
    char dev[PATH_MAX + 64];
    char inst[PATH_MAX + 64];
    snprintf(dev, sizeof(dev), "%s/assets/fonts/%s", exe_dir, FONT_FILE);
    snprintf(inst, sizeof(inst), "%s/fonts/%s", ZEAL_ASSETS_DIR, FONT_FILE);
    candidates[0] = dev;
    candidates[1] = inst;
    candidates[2] = nullptr;

    for (int i = 0; candidates[i]; i++) {
        snprintf(path, sizeof(path), "%s", candidates[i]);
        if (!path_exists(path)) continue;
        if (register_font_file(path)) {
            Fl::set_font(FL_FREE_FONT, FONT_FAMILY);
            DATA_FONT = FL_FREE_FONT;
            DATA_SIZE = 16; /* the font's native 8x16 cell */
            g_pixel_font = true;
            return;
        }
    }
    fprintf(stderr, "[UI] %s not found, using a system monospace font\n", FONT_FILE);
}

void apply_ui_font()
{
    /* Use the system monospace font for UI text (labels, buttons, menus,
     * tooltips, status bar, overlays) by remapping FLTK's default sans faces
     * onto the courier slots, after pointing those at a good system mono. */
#if defined(__APPLE__)
    Fl::set_font(FL_COURIER, "Menlo-Regular");
    Fl::set_font(FL_COURIER_BOLD, "Menlo-Bold");
    Fl::set_font(FL_COURIER_ITALIC, "Menlo-Italic");
    Fl::set_font(FL_COURIER_BOLD_ITALIC, "Menlo-BoldItalic");
#elif defined(_WIN32)
    Fl::set_font(FL_COURIER, " Consolas");
    Fl::set_font(FL_COURIER_BOLD, "BConsolas");
    Fl::set_font(FL_COURIER_ITALIC, "IConsolas");
    Fl::set_font(FL_COURIER_BOLD_ITALIC, "PConsolas");
#endif
    /* Linux/BSD: FLTK's courier slots already resolve to fontconfig "mono" */
    Fl::set_font(FL_HELVETICA, FL_COURIER);
    Fl::set_font(FL_HELVETICA_BOLD, FL_COURIER_BOLD);
    Fl::set_font(FL_HELVETICA_ITALIC, FL_COURIER_ITALIC);
    Fl::set_font(FL_HELVETICA_BOLD_ITALIC, FL_COURIER_BOLD_ITALIC);
}

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

/* ------------------------------------------------------------------ */
/*  IconButton                                                         */
/* ------------------------------------------------------------------ */

IconButton::IconButton(int X, int Y, int W, int H, Icon icon, const char* text)
    : Fl_Button(X, Y, W, H), m_icon(icon), m_text(text)
{
    box(FL_FLAT_BOX);
    clear_visible_focus();
}

void IconButton::draw()
{
    const bool down = value() != 0;
    const bool hover = Fl::belowmouse() == this;
    fl_color(down ? COLOR_BORDER : hover ? COLOR_RAISED : COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    if (hover || down) {
        fl_color(COLOR_BORDER);
        fl_rect(x(), y(), w(), h());
    }
    const Fl_Color c = active_r() ? (m_color ? m_color : COLOR_TEXT) : COLOR_DIM;
    const int s = h() - 10;
    draw_icon(m_icon, x() + 6, y() + 5, s, c);
    if (m_text) {
        fl_font(FL_HELVETICA, 12);
        fl_color(active_r() ? COLOR_TEXT : COLOR_DIM);
        fl_draw(m_text, x() + s + 12, y() + (h() + fl_height()) / 2 - fl_descent());
    }
}

int IconButton::handle(int event)
{
    if (event == FL_ENTER || event == FL_LEAVE) {
        redraw();
        return 1;
    }
    return Fl_Button::handle(event);
}

/* ------------------------------------------------------------------ */
/*  SplitPane                                                          */
/* ------------------------------------------------------------------ */

SplitPane::SplitPane(int X, int Y, int W, int H, bool horizontal, bool fixed_second)
    : Fl_Group(X, Y, W, H), m_horizontal(horizontal), m_fixed_second(fixed_second)
{
    box(FL_FLAT_BOX);
    color(COLOR_BG);
    end();
}

void SplitPane::set(Fl_Widget* a, Fl_Widget* b, int fixed_size, int min_a, int min_b)
{
    m_a = a;
    m_b = b;
    m_fixed = fixed_size;
    m_min_a = min_a;
    m_min_b = min_b;
    add(a);
    add(b);
    layout();
}

void SplitPane::collapse(bool collapsed)
{
    m_collapsed = collapsed;
    if (m_b) {
        if (collapsed) m_b->hide(); else m_b->show();
    }
    layout();
    redraw();
}

void SplitPane::fixed_size(int size)
{
    if (size > 0) m_fixed = size;
    layout();
    redraw();
}

void SplitPane::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    layout();
}

int SplitPane::extent() const
{
    return m_horizontal ? w() : h();
}

void SplitPane::layout()
{
    if (!m_a || !m_b) return;
    if (m_collapsed) {
        m_a->resize(x(), y(), w(), h());
        return;
    }
    const int total = extent() - GAP;
    int fixed = m_fixed;
    const int min_fixed = m_fixed_second ? m_min_b : m_min_a;
    const int min_other = m_fixed_second ? m_min_a : m_min_b;
    if (fixed > total - min_other) fixed = total - min_other;
    if (fixed < min_fixed) fixed = min_fixed;
    const int size_a = m_fixed_second ? total - fixed : fixed;
    const int size_b = total - size_a;
    if (m_horizontal) {
        m_a->resize(x(), y(), size_a, h());
        m_b->resize(x() + size_a + GAP, y(), size_b, h());
    } else {
        m_a->resize(x(), y(), w(), size_a);
        m_b->resize(x(), y() + size_a + GAP, w(), size_b);
    }
}

bool SplitPane::over_divider(int ex, int ey) const
{
    if (m_collapsed || !m_a) return false;
    if (m_horizontal) {
        const int d = m_a->x() + m_a->w();
        return ex >= d && ex < d + GAP && ey >= y() && ey < y() + h();
    }
    const int d = m_a->y() + m_a->h();
    return ey >= d && ey < d + GAP && ex >= x() && ex < x() + w();
}

int SplitPane::handle(int event)
{
    switch (event) {
        case FL_MOVE:
        case FL_ENTER: {
            const bool over = over_divider(Fl::event_x(), Fl::event_y());
            if (over != m_cursor_set && window()) {
                window()->cursor(over ? (m_horizontal ? FL_CURSOR_WE : FL_CURSOR_NS) : FL_CURSOR_DEFAULT);
                m_cursor_set = over;
            }
            if (over) return 1;
            break;
        }
        case FL_LEAVE:
            if (m_cursor_set && !m_dragging && window()) {
                window()->cursor(FL_CURSOR_DEFAULT);
                m_cursor_set = false;
            }
            break;
        case FL_PUSH:
            if (over_divider(Fl::event_x(), Fl::event_y())) {
                m_dragging = true;
                return 1;
            }
            break;
        case FL_DRAG:
            if (m_dragging) {
                const int pos = m_horizontal ? Fl::event_x() - x() : Fl::event_y() - y();
                m_fixed = m_fixed_second ? extent() - pos - GAP / 2 : pos - GAP / 2;
                layout();
                redraw();
                if (m_changed) m_changed();
                return 1;
            }
            break;
        case FL_RELEASE:
            if (m_dragging) {
                m_dragging = false;
                return 1;
            }
            break;
        default:
            break;
    }
    return Fl_Group::handle(event);
}

void SplitPane::draw()
{
    Fl_Group::draw();
    if (m_collapsed || !m_a) return;
    /* A subtle grip in the middle of the divider */
    fl_color(COLOR_BORDER);
    if (m_horizontal) {
        const int d = m_a->x() + m_a->w() + GAP / 2;
        fl_rectf(d, y() + h() / 2 - 14, 1, 28);
    } else {
        const int d = m_a->y() + m_a->h() + GAP / 2;
        fl_rectf(x() + w() / 2 - 14, d, 28, 1);
    }
}

} // namespace zui
