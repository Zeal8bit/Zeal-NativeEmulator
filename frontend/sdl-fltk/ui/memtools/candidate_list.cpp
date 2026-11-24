/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "memtools/candidate_list.h"
#include <cstdint>
#include <cstdio>
#include <FL/Fl.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "theme/theme.h"

namespace zui::memtools {

using namespace zui::common;
using namespace zui::theme;

CandidateList::CandidateList(int X, int Y, int W, int H, Fl_Scrollbar* bar)
    : Fl_Widget(X, Y, W, H), m_bar(bar)
{}

int CandidateList::rows() const
{ return (h() - 4) / data_line_height(); }

int CandidateList::count() const
{ return items ? (int) items->size() : 0; }

void CandidateList::scroll_to(int top)
{
    m_top = top;
    clamp();
    redraw();
}

void CandidateList::sync()
{
    clamp();
    m_bar->value(m_top, rows(), 0, count() > rows() ? count() : rows());
    redraw();
}

void CandidateList::draw()
{
    fl_push_clip(x(), y(), w(), h());
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    const uint8_t* prev = nullptr;
    const uint8_t* cur = nullptr;
    const bool have = source && source->get && source->get(&prev, &cur);
    const int lh = data_line_height();
    const int asc = text_baseline(lh);
    const int cw = char_width();
    fl_font(DATA_FONT, DATA_SIZE);
    char t[16];
    for (int r = 0; r < rows() && m_top + r < count(); r++) {
        const int i = m_top + r;
        const unsigned a = (*items)[(size_t) i];
        const int yy = y() + 2 + r * lh;
        if (i == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(x(), yy, w(), lh);
        }
        int xx = x() + 6;
        hex16(t, sizeof(t), a);
        fl_color(COLOR_DIM);
        fl_draw("$", xx, yy + asc);
        fl_color(COLOR_CYAN);
        fl_draw(t, xx + cw, yy + asc);
        xx += cw * 7;
        if (have) {
            const uint8_t p = prev ? prev[a] : cur[a];
            const uint8_t c = cur[a];
            hex8(t, sizeof(t), p);
            fl_color(COLOR_DIM);
            fl_draw("$", xx, yy + asc);
            fl_color(COLOR_TEXT);
            fl_draw(t, xx + cw, yy + asc);
            fl_color(COLOR_DIM);
            fl_draw("\xE2\x86\x92", xx + cw * 4, yy + asc); /* arrow */
            hex8(t, sizeof(t), c);
            fl_draw("$", xx + cw * 6, yy + asc);
            fl_color(c != p ? COLOR_CHANGED : COLOR_TEXT);
            fl_draw(t, xx + cw * 7, yy + asc);
            snprintf(t, sizeof(t), "#%u", c);
            fl_color(COLOR_DIM);
            fl_draw(t, xx + cw * 10, yy + asc);
            xx += cw * 15;
        }
        const char* sym = debugger_get_symbol(fz_debugger(), (hwaddr) a);
        if (sym) {
            fl_color(COLOR_AMBER);
            fl_draw(sym, xx, yy + asc);
        }
    }
    if (count() == 0) {
        fl_font(FL_HELVETICA, 12);
        fl_color(COLOR_DIM);
        fl_draw(items ? "No candidates left. Reset to start over." : "Pick a filter and press Apply.",
                x() + 8, y() + 4, w() - 16, 40, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
    }
    fl_pop_clip();
}

int CandidateList::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            m_hover = -1;
            redraw();
            return 1;
        case FL_MOVE: {
            const int i = index_at(Fl::event_y());
            if (i != m_hover) {
                m_hover = i;
                redraw();
            }
            return 1;
        }
        case FL_PUSH: {
            const int i = index_at(Fl::event_y());
            if (i >= 0 && source && source->go) {
                source->go((*items)[(size_t) i]);
            }
            return 1;
        }
        case FL_MOUSEWHEEL:
            m_top += Fl::event_dy() * 3;
            sync();
            return 1;
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

void CandidateList::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    sync();
}

int CandidateList::index_at(int ey) const
{
    const int r = (ey - y() - 2) / data_line_height();
    if (ey < y() + 2 || r >= rows()) {
        return -1;
    }
    const int i = m_top + r;
    return i < count() ? i : -1;
}

void CandidateList::clamp()
{
    if (m_top > count() - rows()) {
        m_top = count() - rows();
    }
    if (m_top < 0) {
        m_top = 0;
    }
}

} // namespace zui::memtools
