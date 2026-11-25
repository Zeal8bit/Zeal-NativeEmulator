/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/disasm_panel.h"
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_ask.H>
#include "common/machine.h"
#include "debugger/address_lookup.h"
#include "debugger/disasm_view.h"
#include "debugger/machine_state.h"
#include "theme/style.h"
#include "theme/theme.h"
#include "widgets/panel.h"

namespace zui::debugger {

using namespace zui::theme;

DisasmPanel::DisasmPanel(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "DISASSEMBLY")
{
    Fl_Group::current(this);
    m_tabs = new Fl_Tabs(X, Y + HEADER_H, W, H - HEADER_H);
    m_tabs->box(FL_FLAT_BOX);
    m_tabs->color(COLOR_SURFACE);
    m_tabs->selection_color(COLOR_RAISED);
    m_tabs->labelcolor(COLOR_TEXT);
    m_tabs->begin();

    m_cpu = new Fl_Group(X, Y + HEADER_H + 26, W, H - HEADER_H - 26);
    m_cpu->label("CPU");
    m_cpu->begin();
    m_input = new Fl_Input(X, Y, 10, 10);
    style_input(m_input);
    m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
    m_input->callback(cb_go, this);
    m_input->tooltip("Address (hex) or symbol name, then Enter\nPart of a name lists the matching symbols");
    m_pc = new Fl_Button(X, Y, 10, 10, "PC");
    style_button(m_pc);
    m_pc->callback(cb_pc, this);
    m_pc->tooltip("Jump to the current PC and follow it");
    m_follow = new Fl_Check_Button(X, Y, 10, 10, "Follow PC");
    style_check(m_follow);
    m_follow->value(1);
    m_view = new DisasmView(X, Y, 10, 10);
    m_view->tooltip("Click: select the line    Left edge: toggle a breakpoint    Right-click: more");
    m_cpu->end();

    m_rpu = new Fl_Group(X, Y + HEADER_H + 26, W, H - HEADER_H - 26);
    m_rpu->label("RPU");
    m_rpu->begin();
    m_rpu_view = new RpuDisasmView(X, Y, 10, 10);
    m_rpu_bar = new Fl_Scrollbar(X, Y, 10, 10);
    m_rpu_bar->type(FL_VERTICAL);
    m_rpu_bar->when(FL_WHEN_CHANGED);
    m_rpu_bar->callback(cb_rpu_scroll, this);
    m_rpu_view->set_scrollbar(m_rpu_bar);
    m_rpu->end();

    m_tabs->end();
    resizable(m_tabs);
    end();
    resize(X, Y, W, H);
}

void DisasmPanel::refresh(bool paused, bool force)
{
    if (paused && force && m_follow->value()) {
        m_view->follow(current_regs().pc);
    }
    m_view->redraw();
    m_rpu_view->refresh();
}

void DisasmPanel::go(hwaddr addr)
{
    m_follow->value(0);
    reveal_address(addr);
}

void DisasmPanel::reveal_address(hwaddr addr)
{
    m_tabs->value(m_cpu);
    m_view->selected = (int) addr;
    m_view->go(addr);
}

int DisasmPanel::selected() const
{ return m_view->selected; }

void DisasmPanel::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    m_tabs->resize(X, Y + HEADER_H, W, H - HEADER_H);

    const int tab_h = 26;
    const int cy = m_tabs->y() + tab_h;
    const int ch = m_tabs->h() - tab_h;
    m_cpu->resize(m_tabs->x(), cy, m_tabs->w(), ch);
    m_rpu->resize(m_tabs->x(), cy, m_tabs->w(), ch);

    const int by = m_cpu->y() + 4;
    m_input->resize(m_cpu->x() + 6, by, 140, BAR_H - 8);
    m_pc->resize(m_cpu->x() + 150, by, 40, BAR_H - 8);
    m_follow->resize(m_cpu->x() + 196, by, 96, BAR_H - 8);
    m_view->resize(m_cpu->x(), m_cpu->y() + BAR_H, m_cpu->w(), m_cpu->h() - BAR_H);

    const int bw = 14;
    m_rpu_bar->resize(m_rpu->x() + m_rpu->w() - bw, m_rpu->y(), bw, m_rpu->h());
    m_rpu_view->resize(m_rpu->x(), m_rpu->y(), m_rpu->w() - bw, m_rpu->h());
    m_rpu_view->sync_scrollbar();
}

void DisasmPanel::cb_go(Fl_Widget*, void* data)
{
    DisasmPanel* self = static_cast<DisasmPanel*>(data);
    hwaddr addr;
    if (lookup_address(self->m_input, self->m_input->value(), &addr, SYM_KIND_CODE)) {
        self->go(addr);
    } else {
        fl_beep();
    }
}

void DisasmPanel::cb_rpu_scroll(Fl_Widget* w, void* data)
{
    DisasmPanel* self = static_cast<DisasmPanel*>(data);
    self->m_rpu_view->scroll_to((int) ((Fl_Scrollbar*) w)->value());
}

void DisasmPanel::cb_pc(Fl_Widget*, void* data)
{
    DisasmPanel* self = static_cast<DisasmPanel*>(data);
    self->m_follow->value(1);
    self->m_view->go(current_regs().pc);
}

} // namespace zui::debugger
