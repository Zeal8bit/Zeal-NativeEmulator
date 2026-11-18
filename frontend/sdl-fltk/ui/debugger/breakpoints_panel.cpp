/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/breakpoints_panel.h"
#include <cctype>
#include <cstdio>
#include <string>
#include <utility>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_ask.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/address_lookup.h"
#include "debugger/breakpoint_list.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "debugger/watchpoint_actions.h"
#include "debugger/watchpoint_list.h"
#include "theme/style.h"
#include "theme/theme.h"
#include "widgets/panel.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;
using namespace zui::widgets;

BreakpointsPanel::BreakpointsPanel(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "BREAKPOINTS")
{
    m_list = new BreakpointList(X, Y, 10, 10);
    m_watch = new WatchpointList(X, Y, 10, 10);
    m_watch->tooltip("Diamond: enable/disable    Address: show in memory    x: remove    Right-click: more\n"
                     "Add from the memory view (right-click) or type r/w/rw ADDR below");
    m_input = new Fl_Input(X, Y, 10, 10);
    style_input(m_input);
    m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
    m_input->callback(cb_add, this);
    m_input->tooltip("Address (hex) or symbol name, then Enter\nPart of a name lists the matching symbols\n"
                     "Watchpoint: r, w or rw then an address or LO-HI (w C000, rw 4000-40FF)");
    m_add = new Fl_Button(X, Y, 10, 10, "Add");
    style_button(m_add);
    m_add->callback(cb_add, this);
    m_list->tooltip("Dot: enable/disable    Address: show in disassembly    x: remove    Right-click: more");
    resizable(m_list);
    end();
    resize(X, Y, W, H);
}

void BreakpointsPanel::refresh(bool paused, bool force)
{
    (void) paused;
    (void) force;
    if (!dbg_ready()) return;
    hwaddr bps[DBG_MAX_POINTS];
    const int count = debugger_get_breakpoints(fz_debugger(), bps, DBG_MAX_POINTS);
    m_list->addrs.assign(bps, bps + (count > 0 ? count : 0));
    int off = 0;
    for (hwaddr a : m_list->addrs) off += !debugger_is_breakpoint_enabled(fz_debugger(), a);
    watchpoint_t wps[DBG_MAX_POINTS];
    const int watch_count = debugger_get_watchpoints(fz_debugger(), wps, DBG_MAX_POINTS);
    m_watch->points.assign(wps, wps + (watch_count > 0 ? watch_count : 0));
    char text[48];
    int n = 0;
    if (off > 0) n = snprintf(text, sizeof(text), "%d  (%d OFF)", count, off);
    else n = snprintf(text, sizeof(text), "%d", count > 0 ? count : 0);
    if (watch_count > 0) snprintf(text + n, sizeof(text) - (size_t) n, "  %d WATCH", watch_count);
    extra(text);
    if (m_watch->h() != watch_height()) resize(x(), y(), w(), h());
    m_list->redraw();
    m_watch->redraw();
}

int BreakpointsPanel::watch_height() const
{
    /* Breakpoints keep the rows they need (two for the empty hint), up to
     * half of the area; watchpoints get the rest, at least one row */
    const int area = h() - HEADER_H - BAR_H;
    const int lh = data_line_height();
    int bp_rows = (int) m_list->addrs.size();
    if (bp_rows < 2) bp_rows = 2;
    int bp_h = bp_rows * lh + 4;
    if (bp_h > area / 2) bp_h = area / 2;
    int rows = (area - bp_h - WatchpointList::height_for(0, 1)) / lh + 1;
    if (rows < 1) rows = 1;
    return WatchpointList::height_for((int) m_watch->points.size(), rows);
}

void BreakpointsPanel::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    const int iy = Y + H - BAR_H + 4;
    const int wh = watch_height();
    m_list->resize(X, Y + HEADER_H, W, H - HEADER_H - BAR_H - wh);
    m_watch->resize(X, Y + H - BAR_H - wh, W, wh);
    m_input->resize(X + 6, iy, W - 72, BAR_H - 8);
    m_add->resize(X + W - 62, iy, 56, BAR_H - 8);
}

void BreakpointsPanel::cb_add(Fl_Widget*, void* data)
{
    BreakpointsPanel* self = static_cast<BreakpointsPanel*>(data);
    if (dbg_ready() && self->add_watch(self->m_input->value())) return;
    hwaddr addr;
    std::string name;
    if (dbg_ready() && lookup_address(self->m_input, self->m_input->value(), &addr, SYM_KIND_CODE, &name)) {
        debugger_set_breakpoint(fz_debugger(), addr);
        self->m_input->value("");
        char text[16];
        hex16(text, sizeof(text), addr);
        if (name.empty()) notif_show("Breakpoint at $%s", text);
        else notif_show("Breakpoint at %s ($%s)", name.c_str(), text);
        request_refresh();
    } else {
        fl_beep();
    }
}

bool BreakpointsPanel::add_watch(const char* text)
{
    while (isspace((unsigned char) *text)) text++;
    watchpoint_type_t type;
    size_t skip;
    const int c0 = tolower((unsigned char) text[0]);
    if (c0 == 'r' && tolower((unsigned char) text[1]) == 'w' && isspace((unsigned char) text[2])) {
        type = WATCHPOINT_RW;
        skip = 2;
    } else if ((c0 == 'r' || c0 == 'w') && isspace((unsigned char) text[1])) {
        type = c0 == 'r' ? WATCHPOINT_READ : WATCHPOINT_WRITE;
        skip = 1;
    } else {
        return false;
    }
    std::string rest(text + skip);
    rest.erase(0, rest.find_first_not_of(" \t"));
    rest.erase(rest.find_last_not_of(" \t") + 1);
    hwaddr lo;
    hwaddr hi;
    const size_t dash = rest.find('-');
    if (dash != std::string::npos && parse_address(rest.substr(0, dash).c_str(), &lo) &&
        parse_address(rest.substr(dash + 1).c_str(), &hi)) {
        if (hi < lo) std::swap(lo, hi);
    } else if (lookup_address(m_input, rest.c_str(), &lo, SYM_KIND_DATA)) {
        hi = lo;
    } else {
        fl_beep();
        return true;
    }
    if (lo > 0xFFFF || hi > 0xFFFF) {
        fl_beep();
        return true;
    }
    if (watch_add(lo, hi, type)) m_input->value("");
    return true;
}

} // namespace zui::debugger
