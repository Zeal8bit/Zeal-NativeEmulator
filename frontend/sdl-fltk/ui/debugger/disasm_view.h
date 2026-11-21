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

class DisasmView : public Fl_Widget {
public:
    DisasmView(int X, int Y, int W, int H);

    int selected = -1;

    void go(hwaddr addr);

    /** Keep PC visible without jumping on every step. */
    void follow(hwaddr pc);
    void draw() override;
    int handle(int event) override;

private:
    static constexpr int GUTTER = 22;
    struct Row { int y; int h; hwaddr addr; };

    int row_at(int ey) const;
    void context_menu(hwaddr addr);

    hwaddr m_base = 0;
    std::vector<hwaddr> m_history;
    std::vector<Row> m_rows;
    int m_hover = -1;
};

} // namespace zui::debugger
