/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <functional>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "frontend/sdl-fltk/bridge/console.h"

namespace zui::console {

class ConsoleText : public Fl_Widget {
public:
    ConsoleText(int X, int Y, int W, int H, Fl_Scrollbar* bar);

    bool follow = true;
    bool time = false;
    std::function<void(bool)> on_follow;
    std::function<void()> on_menu;

    int rows() const;

    /** Clamp, apply follow and update the scrollbar. */
    void sync();
    void scroll_to(int rel);
    int selected() const { return m_sel_valid ? 1 : 0; }
    bool selected_line(fz_console_line_t* out) const;
    void cleared();
    void draw() override;
    int handle(int event) override;

private:
    uint64_t line_at(int ey) const;
    void set_follow(bool f);

    Fl_Scrollbar* m_bar;
    uint64_t m_top = 0;
    uint64_t m_sel = 0;
    bool m_sel_valid = false;
    uint64_t m_hover = UINT64_MAX;
};

} // namespace zui::console
