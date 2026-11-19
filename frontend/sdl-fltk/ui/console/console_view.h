/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "console/console_text.h"
#include "widgets/panel.h"

namespace zui::console {

class ConsoleView : public widgets::Panel {
public:

    ConsoleView(int X, int Y, int W, int H);
    ~ConsoleView() override;
    void resize(int X, int Y, int W, int H) override;
    void refresh(bool paused, bool force) override;

private:
    void update();
    void copy_all();
    void clear();
    void save();
    void context_menu();
    void set_follow(bool f);
    void set_time(bool t);
    static void cb_tick(void* data);
    static void cb_follow(Fl_Widget*, void* data);
    static void cb_time(Fl_Widget*, void* data);
    static void cb_clear(Fl_Widget*, void* data);
    static void cb_copy(Fl_Widget*, void* data);
    static void cb_save(Fl_Widget*, void* data);
    static void cb_bar(Fl_Widget*, void* data);

    Fl_Check_Button* m_follow;
    Fl_Check_Button* m_time;
    Fl_Button* m_clear;
    Fl_Button* m_copy;
    Fl_Button* m_save;
    Fl_Scrollbar* m_bar;
    ConsoleText* m_text;
    uint64_t m_serial = UINT64_MAX;
};

} // namespace zui::console
