/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/watchpoint_list.h"
#include <cstddef>
#include <cstdio>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/navigation.h"
#include "debugger/watchpoint_actions.h"
#include "theme/theme.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;

namespace {

constexpr int CAPTION_H = 20;

/* Diamond: filled when enabled, outlined when disabled */
void draw_watch_marker(int X, int Y, int size, bool enabled)
{
    const int h = size / 2;
    fl_color(COLOR_BREAK);
    if (enabled) {
        fl_polygon(X + h, Y, X + size, Y + h, X + h, Y + size, X, Y + h);
    } else {
        fl_line_style(FL_SOLID, 2);
        fl_loop(X + h, Y + 1, X + size - 1, Y + h, X + h, Y + size - 1, X + 1, Y + h);
        fl_line_style(0);
    }
}

} // namespace

WatchpointList::WatchpointList(int X, int Y, int W, int H)
    : Fl_Widget(X, Y, W, H)
{}

int WatchpointList::height_for(int count, int max_rows)
{
    const int rows = count < 1 ? 1 : count > max_rows ? max_rows : count;
    return CAPTION_H + rows * data_line_height() + 2;
}

void WatchpointList::draw()
{
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    fl_push_clip(x(), y(), w(), h());
    fl_color(COLOR_BORDER);
    fl_xyline(x(), y(), x() + w() - 1);
    fl_font(FL_HELVETICA_BOLD, 10);
    fl_color(COLOR_LABEL);
    fl_draw("WATCHPOINTS", x() + 8, y() + CAPTION_H - 6);
    const int lh = data_line_height();
    const int cw = char_width();
    const int asc = text_baseline(lh);
    const int top = y() + CAPTION_H;
    clamp_first();
    const int hidden = (int) points.size() - visible_rows();
    if (hidden > 0) {
        char more[24];
        snprintf(more, sizeof(more), "%d MORE (WHEEL)", hidden);
        fl_font(FL_HELVETICA, 10);
        fl_color(COLOR_DIM);
        fl_draw(more, x() + 8, y(), w() - 16, CAPTION_H - 2, FL_ALIGN_RIGHT | FL_ALIGN_BOTTOM);
    }
    if (points.empty()) {
        fl_color(COLOR_DIM);
        fl_font(FL_HELVETICA, 11);
        fl_draw("Right-click memory: Break on Read / Write.", x() + 8, top, w() - 16, lh,
                FL_ALIGN_LEFT | FL_ALIGN_CLIP);
    }
    fl_font(DATA_FONT, DATA_SIZE);
    char text[24];
    const size_t last = (size_t) (m_first + visible_rows());
    for (size_t i = (size_t) m_first; i < points.size() && i < last; i++) {
        const watchpoint_t& wp = points[i];
        const int ry = top + ((int) i - m_first) * lh;
        const bool enabled = !wp.disabled;
        if ((int) i == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(x(), ry, w(), lh);
            fl_color(COLOR_BREAK);
            fl_draw("x", x() + w() - cw * 2, ry + asc);
        }
        draw_watch_marker(x() + 7, ry + (lh - 11) / 2, 11, enabled);
        watch_range_text(text, sizeof(text), wp.addr, wp.end);
        fl_color(enabled ? COLOR_CYAN : COLOR_DIM);
        fl_draw(text, x() + 26, ry + asc);
        fl_color(enabled ? COLOR_TEXT : COLOR_DIM);
        fl_draw(watch_type_short(wp.type), x() + 26 + cw * 12, ry + asc);
        const char* sym = debugger_get_symbol(fz_debugger(), wp.addr);
        if (sym) {
            fl_color(enabled ? COLOR_AMBER : COLOR_DIM);
            fl_draw(sym, x() + 26 + cw * 15, ry + asc);
        }
        if (!enabled) {
            fl_font(FL_HELVETICA, 10);
            fl_color(COLOR_DIM);
            fl_draw("off", x() + w() - cw * 2 - 30, ry + asc);
            fl_font(DATA_FONT, DATA_SIZE);
        }
    }
    fl_pop_clip();
}

int WatchpointList::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            m_hover = -1;
            redraw();
            return 1;
        case FL_MOVE: {
            const int r = row_at(Fl::event_y());
            if (r != m_hover) {
                m_hover = r;
                redraw();
            }
            return 1;
        }
        case FL_MOUSEWHEEL:
            m_first += Fl::event_dy();
            clamp_first();
            m_hover = row_at(Fl::event_y());
            redraw();
            return 1;
        case FL_PUSH: {
            const int r = row_at(Fl::event_y());
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                context_menu(r);
                return 1;
            }
            if (r < 0) return 1;
            const watchpoint_t wp = points[(size_t) r];
            if (Fl::event_x() < x() + 24) {
                debugger_enable_watchpoint(fz_debugger(), wp.addr, wp.disabled);
                request_refresh();
            } else if (Fl::event_x() >= x() + w() - char_width() * 3) {
                watch_remove(wp.addr);
            } else {
                goto_memory(wp.addr);
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

int WatchpointList::row_at(int ey) const
{
    const int top = y() + CAPTION_H;
    const int r = (ey - top) / data_line_height();
    return ey >= top && r < visible_rows() && m_first + r < (int) points.size() ? m_first + r : -1;
}

int WatchpointList::visible_rows() const
{
    const int rows = (h() - CAPTION_H) / data_line_height();
    return rows < 1 ? 1 : rows;
}

void WatchpointList::clamp_first()
{
    const int max_first = (int) points.size() - visible_rows();
    if (m_first > max_first) m_first = max_first;
    if (m_first < 0) m_first = 0;
}

void WatchpointList::context_menu(int row)
{
    dbg_t* dbg = fz_debugger();
    const bool has_row = row >= 0;
    const watchpoint_t wp = has_row ? points[(size_t) row] : watchpoint_t{};
    const int row_only = has_row ? 0 : FL_MENU_INACTIVE;
    const int any = points.empty() ? FL_MENU_INACTIVE : 0;
    auto radio = [&](int type) { return row_only | FL_MENU_RADIO | (has_row && wp.type == type ? FL_MENU_VALUE : 0); };
    Fl_Menu_Item items[] = {
        { has_row && wp.disabled ? "Enable" : "Disable", 0, nullptr, nullptr, row_only, 0, 0, 0, 0 },
        { "Break on Read", 0, nullptr, nullptr, radio(WATCHPOINT_READ), 0, 0, 0, 0 },
        { "Break on Write", 0, nullptr, nullptr, radio(WATCHPOINT_WRITE), 0, 0, 0, 0 },
        { "Break on Access", 0, nullptr, nullptr, radio(WATCHPOINT_RW) | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Remove", 0, nullptr, nullptr, row_only, 0, 0, 0, 0 },
        { "Show in Memory", 0, nullptr, nullptr, row_only | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Enable All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
        { "Disable All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
        { "Remove All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) return;
    switch ((int) (picked - items)) {
        case 0: debugger_enable_watchpoint(dbg, wp.addr, wp.disabled); break;
        case 1: debugger_set_watchpoint_type(dbg, wp.addr, WATCHPOINT_READ); break;
        case 2: debugger_set_watchpoint_type(dbg, wp.addr, WATCHPOINT_WRITE); break;
        case 3: debugger_set_watchpoint_type(dbg, wp.addr, WATCHPOINT_RW); break;
        case 4: watch_remove(wp.addr); return;
        case 5: goto_memory(wp.addr); return;
        case 6: for (const watchpoint_t& p : points) debugger_enable_watchpoint(dbg, p.addr, true); break;
        case 7: for (const watchpoint_t& p : points) debugger_enable_watchpoint(dbg, p.addr, false); break;
        case 8: debugger_clear_watchpoints(dbg); break;
        default: break;
    }
    request_refresh();
}

} // namespace zui::debugger
