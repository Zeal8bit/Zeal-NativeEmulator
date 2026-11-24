/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "profiler/hot_list.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/navigation.h"
#include "theme/theme.h"

namespace zui::profiler {

using namespace zui::common;
using namespace zui::debugger;
using namespace zui::theme;

/** "12,345" */
/** "$1234" honouring hex_upper */
static void addr_text(char* out, size_t size, hwaddr addr)
{
    char h[8];
    hex16(h, sizeof(h), addr);
    snprintf(out, size, "$%s", h);
}

HotList::HotList(int X, int Y, int W, int H, Fl_Scrollbar* bar)
    : Fl_Widget(X, Y, W, H), m_bar(bar)
{}

int HotList::line_h() const
{ return data_line_height() + 4; }

int HotList::visible_rows() const
{ return std::max(1, (h() - head_h()) / line_h()); }

void HotList::clamp_top()
{
    const int max_top = std::max(0, (int) rows.size() - visible_rows());
    m_top = std::max(0, std::min(m_top, max_top));
}

void HotList::sync_bar()
{
    clamp_top();
    m_bar->value(m_top, visible_rows(), 0, std::max((int) rows.size(), visible_rows()));
}

void HotList::scroll_to_row(int row)
{
    m_top = row;
    clamp_top();
    redraw();
}

void HotList::draw()
{
    fl_push_clip(x(), y(), w(), h());
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());

    if (rows.empty()) {
        draw_empty();
        fl_pop_clip();
        return;
    }

    const int cw = char_width();
    const int lh = line_h();
    const int asc = text_baseline(lh);
    layout_columns(cw);

    /* Column headers */
    fl_font(FL_HELVETICA, 11);
    fl_color(COLOR_DIM);
    const int hy = y() + 15;
    if (addresses) {
        fl_draw("ADDRESS", m_x_addr, hy);
        fl_draw("FUNCTION", m_x_name, hy);
        fl_draw("INSTRUCTION", m_x_instr, hy);
    } else {
        fl_draw("FUNCTION", m_x_name, hy);
        fl_draw("ADDRESS", m_x_addr, hy);
    }
    fl_draw("SAMPLES", m_x_samples_r - (int) fl_width("SAMPLES"), hy);
    fl_draw("%", m_x_bar, hy);
    fl_color(COLOR_BORDER);
    fl_xyline(x() + 6, y() + head_h() - 2, x() + w() - 6);

    fl_font(DATA_FONT, DATA_SIZE);
    char buf[96];
    const int ry0 = y() + head_h();
    const int n = std::min((int) rows.size() - m_top, visible_rows() + 1);
    for (int i = 0; i < n; i++) {
        const HotRow& r = rows[(size_t) (m_top + i)];
        const int ry = ry0 + i * lh;
        const int idx = m_top + i;
        if ((int) r.addr == m_selected) {
            fl_color(COLOR_SELECT);
            fl_rectf(x() + 4, ry, w() - 8, lh);
        } else if (idx == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(x() + 4, ry, w() - 8, lh);
        }
        if (debugger_has_breakpoint(fz_debugger(), r.addr)) {
            fl_color(COLOR_BREAK);
            fl_rectf(x() + 4, ry, 3, lh);
        }

        /* Name (clipped to its column) */
        fl_color(r.symbol ? COLOR_AMBER : COLOR_DIM);
        const int name_w = (addresses ? m_x_instr : m_x_addr) - m_x_name - cw;
        fl_push_clip(m_x_name, ry, std::max(0, name_w), lh);
        fl_draw(r.name.c_str(), m_x_name, ry + asc);
        fl_pop_clip();

        /* Address */
        addr_text(buf, sizeof(buf), r.addr);
        fl_color(COLOR_CYAN);
        fl_draw(buf, m_x_addr, ry + asc);

        /* Disassembly (Addresses mode) */
        if (addresses) {
            dbg_instr_t instr;
            memset(&instr, 0, sizeof(instr));
            debugger_disassemble_address(fz_debugger(), r.addr, &instr);
            fl_color(COLOR_TEXT);
            fl_push_clip(m_x_instr, ry, std::max(0, m_x_samples_r - (int) fl_width("0000000") - m_x_instr), lh);
            fl_draw(instr.instruction, m_x_instr, ry + asc);
            fl_pop_clip();
        }

        /* Samples (decimal) */
        with_commas(buf, sizeof(buf), r.samples);
        fl_color(COLOR_TEXT);
        fl_draw(buf, m_x_samples_r - (int) fl_width(buf), ry + asc);

        /* % with a bar: amber on a dark track */
        const double pct = total ? 100.0 * (double) r.samples / (double) total : 0.0;
        snprintf(buf, sizeof(buf), "%5.1f%%", pct);
        fl_color(COLOR_TEXT);
        fl_draw(buf, m_x_bar, ry + asc);
        const int tx = m_x_bar + cw * 7;
        const int tw = x() + w() - 10 - tx;
        if (tw > 10) {
            const int th = std::max(4, lh / 2 - 1);
            const int ty = ry + (lh - th) / 2;
            fl_color(COLOR_BG);
            fl_rectf(tx, ty, tw, th);
            /* Bars are relative to the hottest row so small shares stay readable */
            const double peak = (double) rows[0].samples;
            const int fw = peak > 0 ? (int) (tw * (double) r.samples / peak + 0.5) : 0;
            if (fw > 0) {
                fl_color(COLOR_AMBER);
                fl_rectf(tx, ty, std::max(1, fw), th);
            }
        }
    }
    fl_pop_clip();
}

int HotList::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            set_hover(-1);
            return 1;
        case FL_MOVE:
            set_hover(row_at(Fl::event_y()));
            return 1;
        case FL_MOUSEWHEEL:
            m_top += Fl::event_dy() * 3;
            sync_bar();
            redraw();
            return 1;
        case FL_PUSH: {
            const int idx = row_at(Fl::event_y());
            if (idx < 0) {
                return 1;
            }
            const HotRow row = rows[(size_t) idx];
            m_selected = (int) row.addr;
            redraw();
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                context_menu(row);
            } else {
                goto_disassembly(row.addr);
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

void HotList::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    sync_bar();
}

void HotList::layout_columns(int cw)
{
    const int left = x() + 12;
    const int right = x() + w() - 10;
    const int bar_w = std::max(cw * 7 + 40, (w() - 24) / 4);
    m_x_bar = right - bar_w;
    m_x_samples_r = m_x_bar - cw * 2;
    if (addresses) {
        m_x_addr = left;
        m_x_name = left + cw * 7;
        m_x_instr = m_x_name + std::max(cw * 14, (m_x_samples_r - m_x_name) * 2 / 5);
    } else {
        m_x_name = left;
        m_x_addr = std::min(left + cw * 28, std::max(left + cw * 14, m_x_samples_r - cw * 18));
    }
}

void HotList::draw_empty()
{
    fl_font(FL_HELVETICA, 13);
    fl_color(COLOR_TEXT);
    fl_draw(empty_title, x() + 16, y() + 16, w() - 32, 22, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_CLIP);
    fl_font(FL_HELVETICA, 11);
    fl_color(COLOR_DIM);
    fl_draw(empty_hint, x() + 16, y() + 42, w() - 32, h() - 50,
            FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP | FL_ALIGN_CLIP);
}

void HotList::set_hover(int idx)
{
    if (idx == m_hover) {
        return;
    }
    m_hover = idx;
    redraw();
}

int HotList::row_at(int ey) const
{
    const int ry0 = y() + head_h();
    if (ey < ry0) {
        return -1;
    }
    const int idx = m_top + (ey - ry0) / line_h();
    return idx < (int) rows.size() ? idx : -1;
}

void HotList::context_menu(const HotRow& row)
{
    dbg_t* dbg = fz_debugger();
    const bool has_bp = debugger_has_breakpoint(dbg, row.addr);
    Fl_Menu_Item items[] = {
        { "Show in Disassembly", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Show in Memory", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { has_bp ? "Remove Breakpoint" : "Add Breakpoint", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Copy Name", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Copy Address", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) {
        return;
    }
    char text[8];
    switch ((int) (picked - items)) {
        case 0:
            goto_disassembly(row.addr);
            break;
        case 1:
            goto_memory(row.addr);
            break;
        case 2:
            debugger_toggle_breakpoint(dbg, row.addr);
            break;
        case 3:
            Fl::copy(row.name.c_str(), (int) row.name.size(), 1);
            break;
        case 4:
            hex16(text, sizeof(text), row.addr);
            Fl::copy(text, (int) strlen(text), 1);
            break;
        default:
            break;
    }
    request_refresh();
}

} // namespace zui::profiler
