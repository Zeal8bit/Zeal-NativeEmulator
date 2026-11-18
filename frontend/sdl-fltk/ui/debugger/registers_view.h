/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "frontend/sdl-fltk/bridge/cpu.h"
#include "widgets/inline_edit.h"
#include "widgets/panel.h"

namespace zui::debugger {

/* Register ids, in display order (two columns) */
enum {
    R_AF, R_BC, R_DE, R_HL, R_IX, R_SP, R_I,
    R_AF_, R_BC_, R_DE_, R_HL_, R_IY, R_PC, R_R,
    R_COUNT
};

class RegistersView : public widgets::Panel {
public:
    RegistersView(int X, int Y, int W, int H);
    static int preferred_height();
    void refresh(bool paused, bool force) override;
    void draw() override;
    int handle(int event) override;

private:
    void cell(int id, int& name_x, int& value_x, int& row_y) const;
    int flags_y() const;
    void chip(int i, int& cx, int& cw) const;

    static constexpr int INT_CHIPS = 6;   /* IM n, IFF1, IFF2, HALT, INT, NMI */
    static constexpr int TIMING_GAP = 9;  /* space holding the divider above the timing lines */

    int int_y() const;
    int timing_y() const;
    void int_chip(int i, int& cx, int& cw) const;
    int hit_int_chip(int ex, int ey) const;
    bool hit_timing(int ey) const;
    static void group_digits(char* out, size_t size, uint64_t value);

    /* CYCLES / FRAME counters and the ZVB raster position (decimal) */
    void draw_timing(int lh, int cw, int asc);
    void tooltip_extra(int hover);

    static constexpr int HOVER_INT = 100;
    static constexpr int HOVER_TIMING = 200;

    int hit_value(int ex, int ey) const;
    int hit_name(int ex, int ey) const;
    int hit_chip(int ex, int ey) const;
    void tooltip_for(int id);
    void context_menu(int id);
    void begin_edit(int id);
    static void cb_commit(Fl_Widget*, void* data);

    widgets::InlineEdit* m_edit;
    int m_edit_id = -1;
    regs_t m_regs = {};
    bool m_paused = false;
    uint16_t m_prev[R_COUNT] = {};
    bool m_changed[R_COUNT] = {};
    bool m_have_prev = false;
    uint8_t m_peek_hl[8] = {};
    uint8_t m_peek_sp[2] = {};
    fz_cpu_state_t m_cpu = {};
    bool m_have_cpu = false;
    int m_hover = -1;
};

} // namespace zui::debugger
