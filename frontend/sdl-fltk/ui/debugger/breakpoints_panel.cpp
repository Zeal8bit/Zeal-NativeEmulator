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
    m_input = new Fl_Input(X, Y, 10, 10);
    style_input(m_input);
    m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
    m_input->callback(cb_add, this);
    m_input->tooltip("Address (hex) or symbol name, then Enter\nPart of a name lists the matching symbols\n"
                     "Break on memory access: from a Watch row or the memory view");
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
    char text[48];
    if (off > 0) snprintf(text, sizeof(text), "%d  (%d OFF)", count, off);
    else snprintf(text, sizeof(text), "%d", count > 0 ? count : 0);
    extra(text);
    m_list->redraw();
}

void BreakpointsPanel::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    const int iy = Y + H - BAR_H + 4;
    m_list->resize(X, Y + HEADER_H, W, H - HEADER_H - BAR_H);
    m_input->resize(X + 6, iy, W - 72, BAR_H - 8);
    m_add->resize(X + W - 62, iy, 56, BAR_H - 8);
}

void BreakpointsPanel::cb_add(Fl_Widget*, void* data)
{
    BreakpointsPanel* self = static_cast<BreakpointsPanel*>(data);
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

} // namespace zui::debugger
