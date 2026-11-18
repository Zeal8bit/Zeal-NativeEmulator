/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "debugger/disasm_view.h"
#include "widgets/panel.h"

namespace zui::debugger {

class DisasmPanel : public widgets::Panel {
public:
    DisasmPanel(int X, int Y, int W, int H);
    void refresh(bool paused, bool force) override;
    void go(hwaddr addr);
    int selected() const;
    void resize(int X, int Y, int W, int H) override;

private:
    static void cb_go(Fl_Widget*, void* data);
    static void cb_pc(Fl_Widget*, void* data);

    Fl_Input* m_input;
    Fl_Button* m_pc;
    Fl_Check_Button* m_follow;
    DisasmView* m_view;
};

} // namespace zui::debugger
