/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/panel_stack.h"
#include <algorithm>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include "theme/theme.h"

namespace zui::widgets {

using namespace zui::theme;

namespace {

/** Smallest height of an expanded panel that shares space. */
int min_height()
{
    return HEADER_H + 2 * data_line_height() + 8;
}

} // namespace

PanelStack::PanelStack(int X, int Y, int W, int H)
    : Fl_Group(X, Y, W, H)
{
    box(FL_FLAT_BOX);
    color(COLOR_BG);
    resizable(nullptr);
}

void PanelStack::add_panel(Panel* panel, int size, bool fill)
{
    if (panel->parent() != this) add(panel);
    m_items.push_back({ panel, size, fill });
    panel->collapsible(true);
    panel->on_collapse = [this](Panel* p) { toggle_collapse(p); };
    panel->on_maximize = [this](Panel* p) { toggle_maximize(p); };
    layout();
}

int PanelStack::size_of(int index) const
{
    return index >= 0 && index < (int) m_items.size() ? m_items[(size_t) index].size : 0;
}

void PanelStack::set_size(int index, int size)
{
    if (index < 0 || index >= (int) m_items.size() || size <= 0) return;
    m_items[(size_t) index].size = std::max(size, min_height());
    layout();
}

bool PanelStack::shown_collapsed(int index) const
{
    if (m_maximized >= 0) return index != m_maximized;
    return m_items[(size_t) index].panel->collapsed();
}

bool PanelStack::flexible(int index) const
{
    return !shown_collapsed(index) && m_items[(size_t) index].panel->preferred_height() <= 0;
}

void PanelStack::toggle_collapse(Panel* panel)
{
    for (size_t i = 0; i < m_items.size(); i++) {
        if (m_items[i].panel != panel) continue;
        if (m_maximized >= 0) {
            /* Leaving the maximised view: the clicked panel opens */
            clear_maximize();
            panel->set_collapsed(false);
        } else {
            panel->set_collapsed(!panel->collapsed());
        }
    }
    layout();
    if (on_change) on_change();
}

void PanelStack::clear_maximize()
{
    m_maximized = -1;
    for (const Item& it : m_items) {
        it.panel->maximized(false);
        it.panel->set_stack_hidden(false);
    }
}

void PanelStack::toggle_maximize(Panel* panel)
{
    int index = -1;
    for (size_t i = 0; i < m_items.size(); i++) {
        if (m_items[i].panel == panel) index = (int) i;
    }
    m_maximized = m_maximized == index ? -1 : index;
    for (size_t i = 0; i < m_items.size(); i++) {
        Panel* p = m_items[i].panel;
        p->maximized((int) i == m_maximized);
        /* Others show their title only; their collapse state is kept */
        p->set_stack_hidden(m_maximized >= 0 && (int) i != m_maximized);
        if ((int) i == m_maximized) p->set_collapsed(false);
    }
    layout();
    if (on_change) on_change();
}

void PanelStack::layout()
{
    const int n = (int) m_items.size();
    if (n == 0) return;
    std::vector<int> hs((size_t) n);
    int total = 0;
    int fill = -1;
    int last_open = -1;
    for (int i = 0; i < n; i++) {
        const Item& it = m_items[(size_t) i];
        if (shown_collapsed(i)) {
            hs[(size_t) i] = HEADER_H;
        } else {
            const int pref = it.panel->preferred_height();
            hs[(size_t) i] = pref > 0 ? pref : std::max(it.size, min_height());
            if (pref <= 0 && (it.fill || fill < 0)) fill = i;
            last_open = i;
        }
        total += hs[(size_t) i];
    }
    const int avail = h() - GAP * (n - 1);
    int diff = avail - total;
    if (m_maximized >= 0) {
        hs[(size_t) m_maximized] += diff;
        diff = 0;
    } else if (fill >= 0) {
        /* The fill panel takes (or gives back) the difference first */
        const int give = std::max(diff, min_height() - hs[(size_t) fill]);
        hs[(size_t) fill] += give;
        diff -= give;
    } else if (last_open >= 0 && diff > 0) {
        hs[(size_t) last_open] += diff; /* nothing shares space: grow the last open one */
        diff = 0;
    }
    /* Still too tall: shrink the other sharing panels, then fixed ones */
    for (int pass = 0; pass < 2 && diff < 0; pass++) {
        for (int i = n - 1; i >= 0 && diff < 0; i--) {
            if (shown_collapsed(i)) continue;
            const bool fixed = m_items[(size_t) i].panel->preferred_height() > 0;
            if ((pass == 0) == fixed) continue;
            const int floor = pass == 0 ? min_height() : HEADER_H;
            const int take = std::min(-diff, hs[(size_t) i] - floor);
            if (take > 0) {
                hs[(size_t) i] -= take;
                diff += take;
            }
        }
    }
    int yy = y();
    for (int i = 0; i < n; i++) {
        m_items[(size_t) i].panel->resize(x(), yy, w(), hs[(size_t) i]);
        yy += hs[(size_t) i] + GAP;
    }
    redraw();
}

void PanelStack::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    layout();
}

int PanelStack::divider_at(int ey) const
{
    for (int i = 0; i + 1 < (int) m_items.size(); i++) {
        const Panel* p = m_items[(size_t) i].panel;
        const int dy = p->y() + p->h();
        if (ey < dy - 2 || ey >= dy + GAP + 2) continue;
        return flexible(i) && flexible(i + 1) ? i : -1;
    }
    return -1;
}

void PanelStack::draw()
{
    Fl_Group::draw();
    /* Grip on draggable dividers */
    for (int i = 0; i + 1 < (int) m_items.size(); i++) {
        if (!flexible(i) || !flexible(i + 1)) continue;
        const Panel* p = m_items[(size_t) i].panel;
        const int gy = p->y() + p->h() + GAP / 2;
        const int cx = x() + w() / 2;
        fl_color(COLOR_BORDER);
        fl_xyline(cx - 12, gy, cx + 12);
    }
}

int PanelStack::handle(int event)
{
    switch (event) {
        case FL_MOVE:
        case FL_ENTER: {
            const bool over = divider_at(Fl::event_y()) >= 0;
            if (over != m_cursor_set && window()) {
                window()->cursor(over ? FL_CURSOR_NS : FL_CURSOR_DEFAULT);
                m_cursor_set = over;
            }
            break;
        }
        case FL_LEAVE:
            if (m_cursor_set && window()) window()->cursor(FL_CURSOR_DEFAULT);
            m_cursor_set = false;
            break;
        case FL_PUSH: {
            const int d = divider_at(Fl::event_y());
            if (d >= 0 && Fl::event_button() == FL_LEFT_MOUSE) {
                /* Resize the panel above, or the one below when the panel
                 * above is the fill one (it follows anyway) */
                m_drag_sign = m_items[(size_t) d].fill ? -1 : 1;
                m_drag = m_drag_sign > 0 ? d : d + 1;
                m_drag_y = Fl::event_y();
                m_drag_size = m_items[(size_t) m_drag].panel->h();
                return 1;
            }
            break;
        }
        case FL_DRAG:
            if (m_drag >= 0) {
                const int dy = Fl::event_y() - m_drag_y;
                const int size = m_drag_size + m_drag_sign * dy;
                m_items[(size_t) m_drag].size = std::max(size, min_height());
                layout();
                return 1;
            }
            break;
        case FL_RELEASE:
            if (m_drag >= 0) {
                m_drag = -1;
                if (on_change) on_change();
                return 1;
            }
            break;
        default:
            break;
    }
    return Fl_Group::handle(event);
}

} // namespace zui::widgets
