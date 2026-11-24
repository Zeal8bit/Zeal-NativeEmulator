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
    resizable(m_view);
    end();
    resize(X, Y, W, H);
}

void DisasmPanel::refresh(bool paused, bool force)
{
    if (paused && force && m_follow->value()) {
        m_view->follow(current_regs().pc);
    }
    m_view->redraw();
}

void DisasmPanel::go(hwaddr addr)
{
    m_follow->value(0);
    reveal_address(addr);
}

void DisasmPanel::reveal_address(hwaddr addr)
{
    m_view->selected = (int) addr;
    m_view->go(addr);
}

int DisasmPanel::selected() const
{ return m_view->selected; }

void DisasmPanel::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    const int by = Y + HEADER_H + 4;
    m_input->resize(X + 6, by, 140, BAR_H - 8);
    m_pc->resize(X + 150, by, 40, BAR_H - 8);
    m_follow->resize(X + 196, by, 96, BAR_H - 8);
    m_view->resize(X, Y + HEADER_H + BAR_H, W, H - HEADER_H - BAR_H);
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

void DisasmPanel::cb_pc(Fl_Widget*, void* data)
{
    DisasmPanel* self = static_cast<DisasmPanel*>(data);
    self->m_follow->value(1);
    self->m_view->go(current_regs().pc);
}

} // namespace zui::debugger
