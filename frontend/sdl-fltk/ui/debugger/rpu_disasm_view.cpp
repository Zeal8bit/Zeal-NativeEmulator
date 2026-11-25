/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */
#include "debugger/rpu_disasm_view.h"
#include <cstdio>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "common/rpu_disasm.h"
#include "frontend/sdl-fltk/frontend.h"
#include "theme/theme.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;

RpuDisasmView::RpuDisasmView(int X, int Y, int W, int H)
    : Fl_Widget(X, Y, W, H)
{}

int RpuDisasmView::rows() const
{
    const int lh = data_line_height();
    const int n = (h() - 8) / (lh > 0 ? lh : 1);
    return n > 0 ? n : 1;
}

int RpuDisasmView::max_top() const
{
    const int max = RPU_PROGRAM_LENGTH - rows();
    return max > 0 ? max : 0;
}

void RpuDisasmView::clamp_top()
{
    if (m_top > max_top()) {
        m_top = max_top();
    }
    if (m_top < 0) {
        m_top = 0;
    }
}

void RpuDisasmView::set_scrollbar(Fl_Scrollbar* bar)
{
    m_bar = bar;
    sync_scrollbar();
}

void RpuDisasmView::sync_scrollbar()
{
    clamp_top();
    if (m_bar != nullptr) {
        m_bar->value(m_top, rows(), 0, RPU_PROGRAM_LENGTH);
    }
}

void RpuDisasmView::scroll_to(int top)
{
    m_top = top;
    sync_scrollbar();
    redraw();
}

void RpuDisasmView::refresh()
{
    sync_scrollbar();
    redraw();
}

void RpuDisasmView::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    sync_scrollbar();
}

void RpuDisasmView::draw()
{
    fl_push_clip(x(), y(), w(), h());
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    if (fltk_machine == nullptr) {
        fl_color(COLOR_DIM);
        fl_font(DATA_FONT, DATA_SIZE);
        fl_draw("NO MACHINE", x() + 6, y() + 4 + text_baseline(data_line_height()));
        fl_pop_clip();
        return;
    }

    const zvb_rpu_t& rpu = fltk_machine->zvb.rpu;
    const int lh = data_line_height();
    const int cw = char_width();
    const int asc = text_baseline(lh);
    const int count = rows();
    const int x_addr = x() + 6;
    const int x_mnem = x_addr + cw * 4;
    const int x_ops = x_mnem + cw * 6;
    fl_font(DATA_FONT, DATA_SIZE);
    for (int i = 0; i < count; i++) {
        const int index = (m_top + i) & (RPU_PROGRAM_LENGTH - 1);
        const bool here = index == rpu.pc;
        const int yy = y() + 4 + i * lh;
        if (here) {
            fl_color(COLOR_PC);
            fl_rectf(x(), yy, w(), lh);
        }
        char label[8];
        snprintf(label, sizeof(label), "%s%02X", here ? ">" : " ", index);
        fl_color(here ? COLOR_GREEN : COLOR_CYAN);
        fl_draw(label, x_addr, yy + asc);

        char mnemonic[8] = {};
        char operands[24] = {};
        rpu_decode(rpu.program[index], mnemonic, sizeof(mnemonic), operands, sizeof(operands));
        fl_color(COLOR_TEXT);
        fl_draw(mnemonic, x_mnem, yy + asc);
        fl_color(COLOR_DIM);
        fl_draw(operands, x_ops, yy + asc);
    }
    fl_pop_clip();
}

int RpuDisasmView::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_MOUSEWHEEL:
            scroll_to(m_top + Fl::event_dy() * 3);
            return 1;
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

} // namespace zui::debugger
