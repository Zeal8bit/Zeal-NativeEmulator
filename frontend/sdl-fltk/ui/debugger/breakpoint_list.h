/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vector>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Scrollbar.H>
#include "common/machine.h"

namespace zui::debugger {

class BreakpointList : public Fl_Widget {
public:
    BreakpointList(int X, int Y, int W, int H, Fl_Scrollbar* bar);

    std::vector<hwaddr> addrs;

    void scroll_to(int top);
    void reveal(hwaddr address);
    void sync_bar();
    void draw() override;
    int handle(int event) override;

private:
    int row_at(int ey) const;
    void context_menu(int row);

    int visible_rows() const;
    void select_row(int row);
    Fl_Scrollbar* m_bar;
    int m_top = 0;
    int m_selection = -1;
    int m_hover = -1;
};

} // namespace zui::debugger
