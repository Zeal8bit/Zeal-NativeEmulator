/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "debugger/mem_find_input.h"
#include "debugger/mem_view.h"
#include "debugger/mmu_strip.h"
#include "widgets/panel.h"
#include "memtools/pattern.h"

namespace zui::debugger {

class MemoryPanel : public widgets::Panel {
public:
    /** Width that shows a full 16-byte row with its ASCII column. */
    static int preferred_width();
    MemoryPanel(int X, int Y, int W, int H);
    ~MemoryPanel() override;
    void resize(int X, int Y, int W, int H) override;
    void refresh(bool paused, bool force) override;
    void go(hwaddr addr);

private:
    void set_input(hwaddr addr);
    void unfollow();

    /** Header: the selection range, else the followed register. */
    void update_extra();
    void show_find(bool show);
    void find_status(const char* text, Fl_Color c);

    /** Search from the selection: Enter on a new query starts at the cursor. */
    void find(bool forward);
    void show_changes();
    static void cb_go(Fl_Widget*, void* data);
    static void cb_follow(Fl_Widget*, void* data);
    static void cb_cp437(Fl_Widget*, void* data);
    static void cb_bar(Fl_Widget*, void* data);
    static void cb_find_toggle(Fl_Widget*, void* data);

    /** Live: highlight matches of the query as it is typed. */
    static void cb_find_changed(Fl_Widget*, void* data);
    static void cb_find_next(Fl_Widget*, void* data);
    static void cb_find_prev(Fl_Widget*, void* data);
    static void cb_changes(Fl_Widget*, void* data);

    Fl_Input* m_input;
    Fl_Choice* m_follow;
    Fl_Check_Button* m_cp437;
    Fl_Button* m_find_btn;
    Fl_Button* m_changes_btn;
    MemFindInput* m_find;
    Fl_Button* m_prev_btn;
    Fl_Button* m_next_btn;
    Fl_Button* m_close_btn;
    Fl_Box* m_find_status;
    MmuStrip* m_mmu;
    Fl_Scrollbar* m_bar;
    MemView* m_view;
    bool m_find_shown = false;
    std::string m_last_query;
    bool m_was_paused = false;
    regs_t m_pause_regs = {};
};

} // namespace zui::debugger
