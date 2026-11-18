/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>
#include "debugger/watch_list.h"
#include "widgets/panel.h"

namespace zui::debugger {

class WatchPanel : public widgets::Panel {
public:
    WatchPanel(int X, int Y, int W, int H);
    void refresh(bool paused, bool force) override;
    void resize(int X, int Y, int W, int H) override;

private:
    /** Re-evaluate after a local edit; keeps the change snapshot. */
    void update();
    void update_extra();
    static void cb_add(Fl_Widget*, void* data);

    WatchList* m_list = nullptr;
    Fl_Input* m_input = nullptr;
    Fl_Choice* m_type = nullptr;
    Fl_Button* m_add = nullptr;
};

} // namespace zui::debugger
