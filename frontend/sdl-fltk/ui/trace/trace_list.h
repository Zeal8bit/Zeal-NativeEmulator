/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The trace table: recorded instructions, newest first, custom-drawn with
 * hover, selection and a scrollbar. Click: show in the disassembly;
 * right-click: menu. */

#pragma once

#include <cstdint>
#include <vector>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "frontend/sdl-fltk/bridge/trace.h"
#include "trace/symbol_index.h"

namespace zui::trace {

class TraceList : public Fl_Widget {
public:
    TraceList(int X, int Y, int W, int H, Fl_Scrollbar* bar);

    /** Replace the rows with a new snapshot (`serial` from fz_trace_serial).
     *  A view scrolled into the past keeps showing the same entries. */
    void set_entries(std::vector<zeal_trace_entry_t>&& entries, uint64_t serial);
    const std::vector<zeal_trace_entry_t>& entries() const { return m_rows; }
    SymbolIndex& symbols() { return m_syms; }

    const char* empty_title = "";
    const char* empty_hint = "";

    int line_h() const;
    int head_h() const { return 22; }
    int visible_rows() const;
    void sync_bar();
    void scroll_to_row(int row);
    void draw() override;
    int handle(int event) override;
    void resize(int X, int Y, int W, int H) override;

private:
    struct Columns {
        int cycle_r, pc, sym, bytes, instr, regs;
        int sym_w, instr_w;
        int reg_count; /* AF BC DE HL IX IY SP that fit */
    };
    Columns layout(int cw) const;
    void clamp_top();
    void draw_empty();
    void set_hover(int idx);
    int row_at(int ey) const;
    /** Address a click goes to: the instruction, or an interrupt's handler. */
    static hwaddr primary_address(const zeal_trace_entry_t& e);
    /** Sequence number of row `idx` (stable while new entries arrive). */
    uint64_t seq_of(int idx) const { return m_serial - 1 - (uint64_t) idx; }
    void context_menu(int idx);

    Fl_Scrollbar* m_bar;
    std::vector<zeal_trace_entry_t> m_rows;
    SymbolIndex m_syms;
    uint64_t m_serial = 0;
    uint64_t m_selected = UINT64_MAX;
    int m_top = 0;
    int m_hover = -1;
};

} // namespace zui::trace
