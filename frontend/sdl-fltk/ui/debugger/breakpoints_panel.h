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
#include "debugger/watchpoint_list.h"
#include "widgets/panel.h"

namespace zui::debugger {

class BreakpointsPanel : public widgets::Panel {
public:
    BreakpointsPanel(int X, int Y, int W, int H);
    void refresh(bool paused, bool force) override;
    void resize(int X, int Y, int W, int H) override;

private:
    static void cb_add(Fl_Widget*, void* data);
    /** "r ADDR", "w ADDR", "rw ADDR" (or "LO-HI"): add a watchpoint.
     *  False when `text` is not a watchpoint (then it is a breakpoint). */
    bool add_watch(const char* text);
    int watch_height() const;

    BreakpointList* m_list;
    WatchpointList* m_watch;
    Fl_Input* m_input;
    Fl_Button* m_add;
};

} // namespace zui::debugger
