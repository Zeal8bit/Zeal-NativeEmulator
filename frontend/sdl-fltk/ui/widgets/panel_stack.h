/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Panels stacked vertically, like an IDE side bar: a click on a title
 * collapses or expands the panel, the maximise button gives one panel the
 * whole column (again to restore). Panels with a preferred_height() keep
 * it; the others share the rest, the "fill" panel taking what is left, and
 * the divider between two of them can be dragged. */
#pragma once

#include <functional>
#include <vector>
#include <FL/Fl_Group.H>
#include "widgets/panel.h"

namespace zui::widgets {

class PanelStack : public Fl_Group {
public:
    static constexpr int GAP = 5;

    PanelStack(int X, int Y, int W, int H);

    /** Add a panel created inside this group; `size` is its starting height
     *  when it shares space, `fill` makes it take what is left. */
    void add_panel(Panel* panel, int size, bool fill = false);
    /** Height of panel `index` when it shares space (persisted). */
    int size_of(int index) const;
    void set_size(int index, int size);
    /** Lay the panels out again (after a collapse or a preferred height change). */
    void layout();
    void toggle_collapse(Panel* panel);
    void clear_maximize();
    void toggle_maximize(Panel* panel);
    /** Collapse state, sizes or maximised panel changed by the user. */
    std::function<void()> on_change;

    void resize(int X, int Y, int W, int H) override;
    void draw() override;
    int handle(int event) override;

private:
    struct Item {
        Panel* panel;
        int size;
        bool fill;
    };
    bool shown_collapsed(int index) const;
    bool flexible(int index) const;
    /** Index of the panel above a draggable divider at `ey`, or -1. */
    int divider_at(int ey) const;

    std::vector<Item> m_items;
    int m_maximized = -1;
    int m_drag = -1;
    int m_drag_y = 0;
    int m_drag_size = 0;
    int m_drag_sign = 1; /* +1: panel above the divider, -1: below */
    bool m_cursor_set = false;
};

} // namespace zui::widgets
