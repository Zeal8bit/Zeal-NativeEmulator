/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/breakpoint_list.h"
#include <algorithm>
#include <cstddef>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/breakpoint_marker.h"
#include "debugger/navigation.h"
#include "theme/theme.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;

BreakpointList::BreakpointList(int X, int Y, int W, int H, Fl_Scrollbar* bar)
    : Fl_Widget(X, Y, W, H), m_bar(bar)
{
    m_bar->callback([](Fl_Widget* w, void* data) {
        static_cast<BreakpointList*>(data)->scroll_to(((Fl_Scrollbar*) w)->value());
    }, this);
}

int BreakpointList::visible_rows() const
{
    return std::max(1, (h() - 2) / data_line_height());
}

void BreakpointList::sync_bar()
{
    const int rows = visible_rows();
    m_top = std::clamp(m_top, 0, std::max(0, (int) addrs.size() - rows));
    m_selection = std::min(m_selection, (int) addrs.size() - 1);
    m_bar->value(m_top, rows, 0, std::max(rows, (int) addrs.size()));
}

void BreakpointList::scroll_to(int top)
{
    m_top = top;
    m_hover = -1;
    sync_bar();
    redraw();
}

void BreakpointList::select_row(int row)
{
    if (addrs.empty()) {
        return;
    }
    m_selection = std::clamp(row, 0, (int) addrs.size() - 1);
    scroll_to(std::clamp(m_top, m_selection - visible_rows() + 1, m_selection));
}

void BreakpointList::reveal(hwaddr address)
{
    auto it = std::find(addrs.begin(), addrs.end(), address);
    if (it != addrs.end()) {
        select_row((int) (it - addrs.begin()));
    }
}

void BreakpointList::draw()
{
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    const int lh = data_line_height();
    const int cw = char_width();
    const int asc = text_baseline(lh);
    fl_push_clip(x(), y(), w(), h());
    if (addrs.empty()) {
        fl_color(COLOR_DIM);
        fl_font(FL_HELVETICA, 11);
        fl_draw("No breakpoints. Click the disassembly gutter or add one below.",
                x() + 8, y() + 4, w() - 16, lh * 2, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
    }
    fl_font(DATA_FONT, DATA_SIZE);
    char text[8];
    for (int i = m_top; i < (int) addrs.size() && i <= m_top + visible_rows(); i++) {
        const int ry = y() + 2 + (i - m_top) * lh;
        const bool enabled = debugger_is_breakpoint_enabled(fz_debugger(), addrs[i]);
        const dbg_t* dbg = fz_debugger();
        const bool hit = fz_paused() && dbg->breakpoint_stopped && dbg->breakpoint_hit_addr == addrs[i];
        const bool hover = i == m_hover || (Fl::focus() == this && i == m_selection);
        if (hit || hover) {
            fl_color(hit ? COLOR_BREAK_ROW : COLOR_HOVER);
            fl_rectf(x(), ry, w(), lh);
        }
        if (hover) {
            fl_color(COLOR_BREAK);
            fl_draw("x", x() + w() - cw * 2, ry + asc);
        }
        draw_breakpoint_marker(x() + 7, ry + (lh - 11) / 2, 11, enabled);
        hex16(text, sizeof(text), addrs[i]);
        fl_color(enabled ? COLOR_CYAN : COLOR_DIM);
        fl_draw(text, x() + 26, ry + asc);
        const char* sym = debugger_get_symbol(fz_debugger(), addrs[i]);
        if (sym) {
            fl_color(enabled ? COLOR_AMBER : COLOR_DIM);
            fl_draw(sym, x() + 26 + cw * 6, ry + asc);
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

int BreakpointList::handle(int event)
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
            scroll_to(m_top + Fl::event_dy() * 3);
            return 1;
        case FL_FOCUS:
            if (m_selection < 0) {
                select_row(m_top);
            }
            redraw();
            return 1;
        case FL_UNFOCUS:
            redraw();
            return 1;
        case FL_KEYDOWN:
            switch (Fl::event_key()) {
                case FL_Up: select_row(m_selection - 1); return 1;
                case FL_Down: select_row(m_selection + 1); return 1;
                case FL_Page_Up: select_row(m_selection - visible_rows()); return 1;
                case FL_Page_Down: select_row(m_selection + visible_rows()); return 1;
                case FL_Home: select_row(0); return 1;
                case FL_End: select_row((int) addrs.size() - 1); return 1;
            }
            if (m_selection < 0 || m_selection >= (int) addrs.size()) {
                break;
            }
            if (Fl::event_key() == ' ') {
                hwaddr addr = addrs[m_selection];
                debugger_enable_breakpoint(fz_debugger(), addr, !debugger_is_breakpoint_enabled(fz_debugger(), addr));
            } else if (Fl::event_key() == FL_Delete) {
                debugger_clear_breakpoint(fz_debugger(), addrs[m_selection]);
            } else {
                break;
            }
            request_refresh();
            return 1;
        case FL_PUSH: {
            const int r = row_at(Fl::event_y());
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                context_menu(r);
                return 1;
            }
            if (r < 0) {
                return 1;
            }
            take_focus();
            select_row(r);
            dbg_t* dbg = fz_debugger();
            if (Fl::event_x() < x() + 24) {
                debugger_enable_breakpoint(dbg, addrs[r], !debugger_is_breakpoint_enabled(dbg, addrs[r]));
                request_refresh();
            } else if (Fl::event_x() >= x() + w() - char_width() * 3) {
                debugger_clear_breakpoint(dbg, addrs[r]);
                request_refresh();
            } else {
                goto_disassembly(addrs[r]);
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

int BreakpointList::row_at(int ey) const
{
    const int r = m_top + (ey - y() - 2) / data_line_height();
    return ey >= y() + 2 && r < (int) addrs.size() ? r : -1;
}

void BreakpointList::context_menu(int row)
{
    dbg_t* dbg = fz_debugger();
    const bool has_row = row >= 0;
    const bool enabled = has_row && debugger_is_breakpoint_enabled(dbg, addrs[row]);
    const int row_only = has_row ? 0 : FL_MENU_INACTIVE;
    const int any = addrs.empty() ? FL_MENU_INACTIVE : 0;
    Fl_Menu_Item items[] = {
        { enabled ? "Disable" : "Enable", 0, nullptr, nullptr, row_only, 0, 0, 0, 0 },
        { "Remove", 0, nullptr, nullptr, row_only, 0, 0, 0, 0 },
        { "Show in Disassembly", 0, nullptr, nullptr, row_only | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Enable All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
        { "Disable All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
        { "Remove All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) {
        return;
    }
    const std::vector<hwaddr> all = addrs;
    switch ((int) (picked - items)) {
        case 0: debugger_enable_breakpoint(dbg, addrs[row], !enabled); break;
        case 1: debugger_clear_breakpoint(dbg, addrs[row]); break;
        case 2: goto_disassembly(addrs[row]); return;
        case 3: for (hwaddr a : all) debugger_enable_breakpoint(dbg, a, true); break;
        case 4: for (hwaddr a : all) debugger_enable_breakpoint(dbg, a, false); break;
        case 5: for (hwaddr a : all) debugger_clear_breakpoint(dbg, a); break;
        default: break;
    }
    request_refresh();
}

} // namespace zui::debugger
