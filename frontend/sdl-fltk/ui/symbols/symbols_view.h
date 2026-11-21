/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "symbols/symbol_list.h"
#include "widgets/filter_input.h"
#include "widgets/panel.h"

namespace zui::symbols {

class SymbolsView : public widgets::Panel {
public:
    SymbolsView(int X, int Y, int W, int H);
    void resize(int X, int Y, int W, int H) override;
    void refresh(bool paused, bool force) override;
    void draw() override;

private:
    void update_extra();
    static const char* base_name(const char* path);
    Fl_Button* add_button(const char* label, const char* tip, Fl_Callback* cb);
    void after_change();
    static void cb_open(Fl_Widget*, void* data);
    static void cb_add(Fl_Widget*, void* data);
    static void cb_reload(Fl_Widget*, void* data);
    static void cb_clear(Fl_Widget*, void* data);
    static void cb_filter(Fl_Widget*, void* data);
    static void cb_kind(Fl_Widget*, void* data);
    static void cb_bar(Fl_Widget*, void* data);

    widgets::FilterInput* m_filter;
    Fl_Choice* m_kind;
    Fl_Button* m_open;
    Fl_Button* m_add;
    Fl_Button* m_reload;
    Fl_Button* m_clear;
    std::string m_reload_tip;
    Fl_Scrollbar* m_bar;
    SymbolList* m_list;
};

} // namespace zui::symbols
