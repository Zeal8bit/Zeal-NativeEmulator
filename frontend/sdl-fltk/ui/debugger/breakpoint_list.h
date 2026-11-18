/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vector>
#include <FL/Fl_Widget.H>
#include "common/machine.h"

namespace zui::debugger {

class BreakpointList : public Fl_Widget {
public:
    BreakpointList(int X, int Y, int W, int H);

    std::vector<hwaddr> addrs;

    void draw() override;
    int handle(int event) override;

private:
    int row_at(int ey) const;
    void context_menu(int row);

    int m_hover = -1;
};

} // namespace zui::debugger
