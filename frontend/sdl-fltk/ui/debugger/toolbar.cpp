/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/toolbar.h"
#include <algorithm>
#include <cstdint>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/machine.h"
#include "debugger/debugger_view.h"
#include "debugger/disasm_panel.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "debugger/state_pill.h"
#include "theme/theme.h"
#include "widgets/icon_button.h"

namespace zui::debugger {

using namespace zui::theme;
using namespace zui::widgets;

Toolbar::Toolbar(int X, int Y, int W, int H)
    : Fl_Group(X, Y, W, H)
{
    box(FL_FLAT_BOX);
    color(COLOR_BG);
    int bx = X + 8;
    const int by = Y + 4;
    const int bh = H - 8;
    auto make = [&](Icon icon, const char* text, int w, Fl_Callback* cb, const char* tip) {
        IconButton* b = new IconButton(bx, by, w, bh, icon, text);
        b->callback(cb);
        b->tooltip(tip);
        bx += w + 4;
        return b;
    };
    m_run = make(Icon::Pause, "Pause", 96, cb_run, "Pause / continue (Cmd/Ctrl+F6, Cmd/Ctrl+F5)");
    m_run->icon_color(COLOR_GREEN);
    make(Icon::StepInto, "Step", 72, cb_step, "Step into (Cmd/Ctrl+F11)");
    make(Icon::StepOver, "Over", 72, cb_step_over, "Step over (Cmd/Ctrl+F10)");
    m_run_to = make(Icon::RunTo, "Run to", 84, cb_run_to, "Run to the line selected in the disassembly");
    bx += 12;
    make(Icon::Reset, "Reset", 76, cb_reset, "Reset the machine (Cmd/Ctrl+Shift+Backspace)");
    m_pill = new StatePill(bx, by, std::max(0, X + W - 8 - bx), bh);
    resizable(nullptr);
    end();
}

void Toolbar::resize(int X, int Y, int W, int H)
{
    const int dx = X - x();
    const int dy = Y - y();
    Fl_Widget::resize(X, Y, W, H);
    for (int i = 0; i < children(); i++) {
        Fl_Widget* c = child(i);
        if (c == m_pill) continue;
        c->resize(c->x() + dx, c->y() + dy, c->w(), c->h());
    }
    const int left = m_pill->x() + dx;
    m_pill->resize(left, Y + 4, std::max(0, X + W - 8 - left), H - 8);
}

void Toolbar::update(bool paused, uint16_t pc, const char* reason)
{
    m_run->icon(paused ? Icon::Play : Icon::Pause);
    m_run->text(paused ? "Continue" : "Pause");
    if (paused) m_run_to->activate(); else m_run_to->deactivate();
    m_pill->set(paused, pc, reason);
}

void Toolbar::draw()
{
    Fl_Group::draw();
    fl_color(COLOR_BORDER);
    fl_xyline(x(), y() + h() - 1, x() + w() - 1);
}

void Toolbar::cb_run(Fl_Widget*, void*)
{
    if (!dbg_ready()) return;
    if (fz_paused()) debugger_continue(fz_debugger()); else debugger_pause(fz_debugger());
    request_refresh();
}

void Toolbar::cb_step(Fl_Widget*, void*)
{ if (dbg_ready()) debugger_step(fz_debugger()); }

void Toolbar::cb_step_over(Fl_Widget*, void*)
{ if (dbg_ready()) debugger_step_over(fz_debugger()); }

void Toolbar::cb_reset(Fl_Widget*, void*)
{
    fz_reset();
    request_refresh();
}

void Toolbar::cb_run_to(Fl_Widget*, void*)
{
    DebuggerView* debugger = DebuggerView::instance();
    if (debugger == nullptr || !dbg_ready() || !fz_paused()) return;
    const int addr = debugger->disasm()->selected();
    if (addr < 0) {
        notif_show("Select a line in the disassembly first");
        return;
    }
    run_to_address((hwaddr) addr);
}

} // namespace zui::debugger
