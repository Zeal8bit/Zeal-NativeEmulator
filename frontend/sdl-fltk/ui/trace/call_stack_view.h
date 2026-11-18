/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Code & Profiler > Call Stack: where the CPU is and the calls (CALL, RST,
 * interrupts) that led there, innermost first. Derived by the core's trace
 * from CALL/RST/interrupt entries and SP moving past return addresses, so it
 * is a heuristic: code that manipulates return addresses can fool it. */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "frontend/sdl-fltk/bridge/trace.h"
#include "trace/symbol_index.h"
#include "widgets/panel.h"

namespace zui::trace {

class CallStackView : public widgets::Panel {
public:
    CallStackView(int X, int Y, int W, int H);
    void refresh(bool paused, bool force) override;
    void draw() override;
    int handle(int event) override;

private:
    /** Row 0 is the current PC, then one row per frame, innermost first. */
    struct Row {
        uint16_t addr;      /* PC, or the call site / interrupted address */
        bool current;
        zeal_frame_t frame; /* the call made from `addr` (unused for the current row) */
    };
    void draw_row(const Row& row, int index, int ry, int lh, int asc, int cw);
    int rows_visible() const;
    int row_at(int ey) const;
    void clamp_first();
    std::string row_text(const Row& row, int index) const;
    void context_menu(int idx);

    std::vector<Row> m_rows;
    bool m_truncated = false;
    SymbolIndex m_syms;
    int m_first = 0;
    int m_hover = -1;
    int m_selected = -1;
};

} // namespace zui::trace
