/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/tabbed_panel.h"
#include <cstdio>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include "theme/theme.h"

namespace zui::widgets {

using namespace zui::theme;

constexpr int TAB_PAD = 10;

TabbedPanel::TabbedPanel(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "")
{}

void TabbedPanel::add_tab(Panel* panel)
{
    panel->show_header(false);
    m_tabs.push_back(panel);
    select(m_current);
}

void TabbedPanel::select(int index)
{
    if (m_tabs.empty()) {
        return;
    }
    if (index < 0 || index >= (int) m_tabs.size()) {
        index = 0;
    }
    m_current = index;
    for (size_t i = 0; i < m_tabs.size(); i++) {
        if ((int) i == index && !content_hidden()) {
            m_tabs[i]->show();
        } else {
            m_tabs[i]->hide();
        }
    }
    resize(x(), y(), w(), h());
    redraw();
}

void TabbedPanel::set_collapsed(bool collapsed)
{
    Panel::set_collapsed(collapsed);
    if (!collapsed) {
        select(m_current); /* only the current tab is shown */
    }
}

void TabbedPanel::refresh(bool paused, bool force)
{
    /* Every tab stays current so that switching shows fresh data and the
     * counts in the labels are right */
    for (Panel* tab : m_tabs) tab->refresh(paused, force);
    damage(FL_DAMAGE_ALL, x(), y(), w(), HEADER_H);
}

void TabbedPanel::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    for (Panel* tab : m_tabs) tab->resize(X, Y + HEADER_H, W, H > HEADER_H ? H - HEADER_H : 0);
}

void TabbedPanel::label(int index, char* out, size_t size) const
{
    const Panel* tab = m_tabs[(size_t) index];
    const char* extra = tab->extra_text();
    if (extra && *extra) {
        snprintf(out, size, "%s %s", tab->title(), extra);
    } else {
        snprintf(out, size, "%s", tab->title());
    }
}

void TabbedPanel::tab_rect(int index, int& tx, int& tw) const
{
    fl_font(DATA_FONT, DATA_SIZE);
    char text[64];
    tx = x() + 14 + 4; /* after the collapse chevron */
    for (int i = 0; i <= index; i++) {
        label(i, text, sizeof(text));
        tw = (int) fl_width(text) + 2 * TAB_PAD;
        if (i < index) {
            tx += tw + 2;
        }
    }
}

int TabbedPanel::tab_at(int ex, int ey) const
{
    if (ey < y() || ey >= y() + HEADER_H) {
        return -1;
    }
    for (int i = 0; i < (int) m_tabs.size(); i++) {
        int tx, tw;
        tab_rect(i, tx, tw);
        if (ex >= tx && ex < tx + tw) {
            return i;
        }
    }
    return -1;
}

void TabbedPanel::draw_header_content(int, int hy, int)
{
    char text[64];
    for (int i = 0; i < (int) m_tabs.size(); i++) {
        int tx, tw;
        tab_rect(i, tx, tw);
        label(i, text, sizeof(text));
        const bool on = i == m_current;
        if (on) {
            fl_color(COLOR_SURFACE);
            fl_rectf(tx, hy + 3, tw, HEADER_H - 4);
            fl_color(COLOR_AMBER);
            fl_rectf(tx, hy + HEADER_H - 3, tw, 2);
        }
        fl_font(DATA_FONT, DATA_SIZE);
        fl_color(on ? COLOR_AMBER : COLOR_DIM);
        fl_draw(text, tx + TAB_PAD, hy + HEADER_H - 1 - (HEADER_H - fl_height()) / 2 - fl_descent());
    }
}

int TabbedPanel::handle(int event)
{
    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
        const int tab = tab_at(Fl::event_x(), Fl::event_y());
        if (tab >= 0) {
            /* A tab of a collapsed panel opens it on that tab */
            if (collapsed() && on_collapse) {
                on_collapse(this);
            }
            if (tab != m_current) {
                select(tab);
                if (on_select) {
                    on_select(tab);
                }
            }
            return 1;
        }
    }
    return Panel::handle(event);
}

} // namespace zui::widgets
