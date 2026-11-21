/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <FL/Fl_Widget.H>
#include "debugger/watch_model.h"

namespace zui::debugger {

class WatchList : public Fl_Widget {
public:
    WatchList(int X, int Y, int W, int H);

    /** Called after an edit of the list (count changed, type changed...). */
    std::function<void()> changed;

    static int head_h() { return 16; }
    void draw() override;
    int handle(int event) override;

    /** Scroll so that the last entry is visible (after adding one). */
    void show_last();
    int first() const { return m_first; }

private:
    static constexpr int BITS_FONT = 11;
    static constexpr int GUTTER = 18; /* break marker column */

    /** Gutter click: no break -> break on write, else enable/disable. */
    void toggle_break(int row);

    /** Bitfield digits use the small UI font to fit the DEC column. */
    static int bits_width();

    /** Bits 7..0 as 0/1, a small gap between the nibbles; set bits lit,
     *  bits that changed since the last pause in the change colour. */
    static void draw_bits(const Watch& wt, int bx, int baseline);
    int rows_y() const;
    int visible_rows() const;
    void clamp_first();
    int row_at(int ey) const;
    void set_hover(int r);
    void context_menu(int row);

    int m_hover = -1;
    int m_first = 0; /* first entry shown (mouse wheel) */
};

} // namespace zui::debugger
