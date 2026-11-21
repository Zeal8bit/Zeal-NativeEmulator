/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The table: custom-drawn rows with hover, selection and a scrollbar */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"

namespace zui::profiler {

struct HotRow {
    hwaddr addr;        /* function start / bucket start / PC */
    hwaddr end;         /* last address covered (inclusive) */
    std::string name;   /* symbol, symbol+offset or bucket range */
    bool symbol;        /* name is a real symbol (amber) */
    uint64_t samples;
};

class HotList : public Fl_Widget {
public:
    HotList(int X, int Y, int W, int H, Fl_Scrollbar* bar);

    std::vector<HotRow> rows;
    uint64_t total = 0;
    bool addresses = false;   /* Addresses mode: show disassembly column */
    const char* empty_title = "";
    const char* empty_hint = "";

    int line_h() const;
    int head_h() const { return 22; }
    int visible_rows() const;
    void clamp_top();
    void sync_bar();
    void scroll_to_row(int row);
    void draw() override;
    int handle(int event) override;
    void resize(int X, int Y, int W, int H) override;

private:
    void layout_columns(int cw);
    void draw_empty();
    void set_hover(int idx);
    int row_at(int ey) const;
    void context_menu(const HotRow& row);

    Fl_Scrollbar* m_bar;
    int m_top = 0;
    int m_hover = -1;
    int m_selected = -1;
    int m_x_name = 0, m_x_addr = 0, m_x_instr = 0, m_x_samples_r = 0, m_x_bar = 0;
};

} // namespace zui::profiler
