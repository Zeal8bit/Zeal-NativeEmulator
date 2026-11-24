/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "console/console_text.h"
#include <cstdint>
#include <string>
#include <FL/Fl.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "console/console_format.h"
#include "frontend/sdl-fltk/bridge/console.h"
#include "theme/theme.h"

namespace zui::console {

using namespace zui::common;
using namespace zui::theme;

ConsoleText::ConsoleText(int X, int Y, int W, int H, Fl_Scrollbar* bar)
    : Fl_Widget(X, Y, W, H), m_bar(bar)
{}

int ConsoleText::rows() const
{
    const int lh = data_line_height();
    return lh > 0 ? (h() - 8) / lh : 1;
}

void ConsoleText::sync()
{
    const uint64_t first = fz_console_first();
    const int count = fz_console_count();
    const int max_top = count > rows() ? count - rows() : 0;
    if (follow || m_top < first) {
        m_top = first + (uint64_t) (follow ? max_top : 0);
    }
    if (m_top > first + (uint64_t) max_top) {
        m_top = first + (uint64_t) max_top;
    }
    m_bar->value((int) (m_top - first), rows(), 0, count > rows() ? count : rows());
}

void ConsoleText::scroll_to(int rel)
{
    const int count = fz_console_count();
    const int max_top = count > rows() ? count - rows() : 0;
    if (rel < 0) {
        rel = 0;
    }
    if (rel > max_top) {
        rel = max_top;
    }
    m_top = fz_console_first() + (uint64_t) rel;
    set_follow(rel >= max_top);
    sync();
    redraw();
}

bool ConsoleText::selected_line(fz_console_line_t* out) const
{
    return m_sel_valid && fz_console_line(m_sel, out);
}

void ConsoleText::cleared()
{
    m_top = 0;
    m_sel_valid = false;
    set_follow(true);
}

void ConsoleText::draw()
{
    fl_push_clip(x(), y(), w(), h());
    fl_color(COLOR_BG);
    fl_rectf(x(), y(), w(), h());
    fl_font(DATA_FONT, DATA_SIZE);
    const int lh = data_line_height();
    const int asc = text_baseline(lh);
    const int cw = char_width();
    const int tx = x() + 8 + (time ? cw * 12 : 0);
    const int count = fz_console_count();
    const uint64_t first = fz_console_first();
    if (count == 0) {
        fl_color(COLOR_DIM);
        fl_draw("No UART output yet. Bytes the machine sends on the UART appear here.",
                x() + 8, y() + 4 + asc);
    }
    std::string text;
    char ts[32];
    for (int r = 0; r <= rows(); r++) {
        const uint64_t n = m_top + (uint64_t) r;
        if (n >= first + (uint64_t) count) {
            break;
        }
        fz_console_line_t line;
        if (!fz_console_line(n, &line)) {
            continue;
        }
        const int ry = y() + 4 + r * lh;
        if (m_sel_valid && n == m_sel) {
            fl_color(COLOR_SELECT);
            fl_rectf(x(), ry, w(), lh);
        } else if (n == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(x(), ry, w(), lh);
        }
        if (time) {
            format_time(ts, sizeof(ts), line.time_ns);
            fl_color(COLOR_DIM);
            fl_draw(ts, x() + 8, ry + asc);
        }
        text.clear();
        for (int i = 0; i < line.len; i++) append_char(text, (uint8_t) line.text[i]);
        fl_color(COLOR_TEXT);
        fl_draw(text.c_str(), (int) text.size(), tx, ry + asc);
    }
    if (time) {
        fl_color(COLOR_BORDER);
        fl_yxline(tx - cw, y(), y() + h());
    }
    fl_pop_clip();
}

int ConsoleText::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            m_hover = UINT64_MAX;
            redraw();
            return 1;
        case FL_MOVE: {
            const uint64_t n = line_at(Fl::event_y());
            if (n != m_hover) {
                m_hover = n;
                redraw();
            }
            return 1;
        }
        case FL_MOUSEWHEEL: {
            const int rel = (int) (m_top - fz_console_first());
            scroll_to(rel + Fl::event_dy() * 3);
            return 1;
        }
        case FL_PUSH: {
            const uint64_t n = line_at(Fl::event_y());
            if (n != UINT64_MAX) {
                m_sel = n;
                m_sel_valid = true;
            } else if (Fl::event_button() != FL_RIGHT_MOUSE) {
                m_sel_valid = false;
            }
            redraw();
            if (Fl::event_button() == FL_RIGHT_MOUSE && on_menu) {
                on_menu();
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

uint64_t ConsoleText::line_at(int ey) const
{
    const int lh = data_line_height();
    if (lh <= 0 || ey < y() + 4) {
        return UINT64_MAX;
    }
    const uint64_t n = m_top + (uint64_t) ((ey - y() - 4) / lh);
    return n < fz_console_first() + (uint64_t) fz_console_count() ? n : UINT64_MAX;
}

void ConsoleText::set_follow(bool f)
{
    if (f == follow) {
        return;
    }
    follow = f;
    if (on_follow) {
        on_follow(f);
    }
}

} // namespace zui::console
