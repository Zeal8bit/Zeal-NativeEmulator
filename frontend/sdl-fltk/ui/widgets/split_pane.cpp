/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/split_pane.h"
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include "theme/theme.h"

namespace zui::widgets {

using namespace zui::theme;

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

void SplitPane::squeeze_second(int size)
{
    m_squeeze = size > 0 ? size : 0;
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
    const int total = extent() > GAP ? extent() - GAP : 0;
    if (m_squeeze > 0) {
        const int size_b = m_squeeze < total ? m_squeeze : total;
        if (m_horizontal) {
            m_a->resize(x(), y(), total - size_b, h());
            m_b->resize(x() + total - size_b + GAP, y(), size_b, h());
        } else {
            m_a->resize(x(), y(), w(), total - size_b);
            m_b->resize(x(), y() + total - size_b + GAP, w(), size_b);
        }
        return;
    }
    int fixed = m_fixed;
    const int min_fixed = m_fixed_second ? m_min_b : m_min_a;
    const int min_other = m_fixed_second ? m_min_a : m_min_b;
    if (min_fixed + min_other > total) {
        /* Less room than both minimums: share it in their proportion, so
         * neither side gets a negative size */
        const int sum = min_fixed + min_other;
        fixed = sum > 0 ? (int) ((long) total * min_fixed / sum) : total / 2;
    } else {
        if (fixed > total - min_other) fixed = total - min_other;
        if (fixed < min_fixed) fixed = min_fixed;
    }
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
    if (m_collapsed || m_squeeze > 0 || !m_a) return false;
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

} // namespace zui::widgets
