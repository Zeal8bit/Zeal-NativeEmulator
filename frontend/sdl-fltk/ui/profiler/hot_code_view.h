/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <vector>
#include <FL/Fl_Button.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "profiler/hot_list.h"
#include "trace/symbol_index.h"
#include "widgets/icon_button.h"
#include "widgets/mode_button.h"
#include "widgets/panel.h"

namespace zui::profiler {

class HotCodeView : public widgets::Panel {
public:
    HotCodeView(int X, int Y, int W, int H);
    void resize(int X, int Y, int W, int H) override;
    void refresh(bool paused, bool force) override;
    void draw() override;

private:
    static void cb_start(Fl_Widget*, void* data);
    static void cb_reset(Fl_Widget*, void* data);
    static void cb_mode(Fl_Widget*, void* data);
    static void cb_bar(Fl_Widget*, void* data);
    void update_controls();
    void update_status();

    void rebuild();
    void set_empty_text();

    widgets::IconButton* m_start;
    Fl_Button* m_reset;
    widgets::ModeButton* m_funcs;
    widgets::ModeButton* m_addrs;
    Fl_Scrollbar* m_bar;
    HotList* m_list;
    trace::SymbolIndex m_index{false};  /* profiler labels include data symbols */
    uint64_t m_last_total = UINT64_MAX;
    int m_shown_running = -1;   /* force the first update */
    char m_status[64] = "";
};

} // namespace zui::profiler
