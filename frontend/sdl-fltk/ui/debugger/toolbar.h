/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include "debugger/state_pill.h"
#include "widgets/icon_button.h"

namespace zui::debugger {

class Toolbar : public Fl_Group {
public:
    Toolbar(int X, int Y, int W, int H);
    void resize(int X, int Y, int W, int H) override;
    /** `reason`: why the machine stopped (watchpoint hit), or nullptr. */
    void update(bool paused, uint16_t pc, const char* reason = nullptr);
    void draw() override;
    static void cb_run_to(Fl_Widget*, void*);

private:
    static void cb_run(Fl_Widget*, void*);
    static void cb_step(Fl_Widget*, void*);
    static void cb_step_over(Fl_Widget*, void*);
    static void cb_reset(Fl_Widget*, void*);

    widgets::IconButton* m_run;
    widgets::IconButton* m_run_to;
    StatePill* m_pill;
};

} // namespace zui::debugger
