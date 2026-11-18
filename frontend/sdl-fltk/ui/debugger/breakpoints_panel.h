/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>
#include "debugger/breakpoint_list.h"
#include "widgets/panel.h"

namespace zui::debugger {

class BreakpointsPanel : public widgets::Panel {
public:
    BreakpointsPanel(int X, int Y, int W, int H);
    void refresh(bool paused, bool force) override;
    void resize(int X, int Y, int W, int H) override;

private:
    static void cb_add(Fl_Widget*, void* data);

    Fl_Scrollbar* m_bar;
    BreakpointList* m_list;
    Fl_Input* m_input;
    Fl_Button* m_add;
};

} // namespace zui::debugger
