/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "memtools/pattern.h"

namespace zui::debugger {

class MemView : public Fl_Widget {
public:
    static constexpr int COLS = 16;
    static constexpr int ROWS_TOTAL = 0x10000 / COLS;
    static constexpr Fl_Color COLOR_MATCH = 0x3D3320'00; /* search hits */

    MemView(int X, int Y, int W, int H, Fl_Scrollbar* bar);

    bool cp437 = false;
    int follow_addr = -1;
    /* Hooks set by the panel */
    std::function<void()> on_selection;
    std::function<void()> on_find;
    std::function<void()> on_changes;
    std::function<void(hwaddr)> on_goto;

    /** Selection: from the anchor to the cursor byte, inclusive. */
    int sel_lo() const;
    int sel_hi() const { return m_anchor > m_sel ? m_anchor : m_sel; }
    int sel_len() const;
    int top_addr() const;

    /** Show `addr`; `select` moves the cursor there, `extend` keeps the anchor. */
    void go(hwaddr addr, bool select = true, bool extend = false);

    /** Select [lo, hi] and bring it into view (a third down when scrolling). */
    void select_range(int lo, int hi);

    /** Tint every visible occurrence of `p` (empty pattern: none). */
    void highlight(const zui::memtools::Pattern& p);
    void snapshot();
    bool snapshots(const uint8_t** prev, const uint8_t** cur) const;

    /** Write through the debugger; edits do not count as "changed". */
    void write(unsigned addr, const std::vector<uint8_t>& data);
    void scroll_to_row(int row);
    int visible_rows() const;
    void sync_bar();
    void draw() override;
    int handle(int event) override;
    void resize(int X, int Y, int W, int H) override;

private:
    int hex_offset(int c) const { return c * m_cw * 3 + (c >= 8 ? m_cw : 0); }
    void geometry(int cw);
    void selection_changed();

    /** Text-column glyph of a byte (UTF-8). */
    void glyph(uint8_t b, char* ch) const;

    /** Flag bytes of [start, start+len) covered by a search-pattern match. */
    void mark_matches(int start, int len, bool* out);
    std::vector<uint8_t> selected_bytes() const;
    static void copy_text(const std::string& text);
    void copy_hex() const;
    void context_menu();
    int addr_at(int ex, int ey);
    int key(int k, const char* text);
    void clamp_top();

    Fl_Scrollbar* m_bar;
    int m_top = 0;
    int m_sel = -1;    /* cursor byte */
    int m_anchor = -1; /* other end of the selection */
    int m_hover = -1;
    int m_nibble = -1;
    int m_cw = 8;
    int m_hex_x = 0;
    int m_ascii_x = 0;
    bool m_dragging = false;
    zui::memtools::Pattern m_find;
    std::vector<uint8_t> m_prev;
    std::vector<uint8_t> m_snapshot;
    bool m_have_prev = false;
    bool m_have_snapshot = false;
};

} // namespace zui::debugger
