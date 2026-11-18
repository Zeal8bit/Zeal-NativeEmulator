/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Code & Profiler > Trace: the last instructions executed ("how did I get
 * here"), newest first, with interrupts and resets as events. */

#pragma once

#include <cstdint>
#include <FL/Fl_Button.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "trace/trace_list.h"
#include "widgets/icon_button.h"
#include "widgets/panel.h"

namespace zui::trace {

class TraceView : public widgets::Panel {
public:
    TraceView(int X, int Y, int W, int H);
    void resize(int X, int Y, int W, int H) override;
    void refresh(bool paused, bool force) override;
    void draw() override;

private:
    static void cb_record(Fl_Widget*, void* data);
    static void cb_clear(Fl_Widget*, void* data);
    static void cb_save(Fl_Widget*, void* data);
    static void cb_bar(Fl_Widget*, void* data);
    void update_controls();
    void update_status();
    void snapshot();
    void set_empty_text();

    widgets::IconButton* m_record;
    Fl_Button* m_clear;
    Fl_Button* m_save;
    Fl_Scrollbar* m_bar;
    TraceList* m_list;
    uint64_t m_serial = UINT64_MAX;
    int m_shown_enabled = -1;   /* force the first update */
    char m_status[64] = "";
};

} // namespace zui::trace
