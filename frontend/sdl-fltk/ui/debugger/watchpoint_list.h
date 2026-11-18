/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Memory watchpoints under the breakpoints: a caption line, then one row
 * per watchpoint (marker, range, R/W/RW, symbol). */
#pragma once

#include <vector>
#include <FL/Fl_Widget.H>
#include "common/machine.h"

namespace zui::debugger {

class WatchpointList : public Fl_Widget {
public:
    WatchpointList(int X, int Y, int W, int H);

    std::vector<watchpoint_t> points;

    /** Height that shows the caption and up to `max_rows` rows. */
    static int height_for(int count, int max_rows);
    void draw() override;
    int handle(int event) override;

private:
    int row_at(int ey) const;
    void context_menu(int row);

    int visible_rows() const;
    void clamp_first();

    int m_hover = -1;
    int m_first = 0; /* first row shown (mouse wheel scrolls) */
};

} // namespace zui::debugger
