/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "trace/trace_list.h"
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/navigation.h"
#include "theme/theme.h"
#include "trace/symbol_index.h"
#include "trace/trace_text.h"

namespace zui::trace {

using namespace zui::common;
using namespace zui::debugger;
using namespace zui::theme;

/* Most useful first: columns are dropped from the right when narrow */
constexpr const char* REG_NAMES[7] = { "AF", "BC", "DE", "HL", "SP", "IX", "IY" };

static uint16_t reg_value(const zeal_trace_entry_t& e, int i)
{
    const uint16_t regs[7] = { e.af, e.bc, e.de, e.hl, e.sp, e.ix, e.iy };
    return regs[i];
}

static Fl_Color event_color(uint8_t kind)
{
    switch (kind) {
        case ZEAL_TRACE_RESET: return COLOR_BREAK;
        case ZEAL_TRACE_GAP: return COLOR_DIM;
        default: return COLOR_GREEN;
    }
}

TraceList::TraceList(int X, int Y, int W, int H, Fl_Scrollbar* bar)
    : Fl_Widget(X, Y, W, H), m_bar(bar)
{}

void TraceList::set_entries(std::vector<zeal_trace_entry_t>&& entries, uint64_t serial)
{
    /* Scrolled into the past: keep the same entries in view as new ones
     * arrive on top. At the top: follow the newest. */
    if (m_top > 0 && serial > m_serial) {
        const uint64_t added = serial - m_serial;
        m_top = added > (uint64_t) entries.size() ? (int) entries.size() : m_top + (int) added;
    }
    m_rows = std::move(entries);
    m_serial = serial;
    m_syms.update();
    sync_bar();
    redraw();
}

int TraceList::line_h() const
{ return data_line_height() + 4; }

int TraceList::visible_rows() const
{ return std::max(1, (h() - head_h()) / line_h()); }

void TraceList::clamp_top()
{
    const int max_top = std::max(0, (int) m_rows.size() - visible_rows());
    m_top = std::max(0, std::min(m_top, max_top));
}

void TraceList::sync_bar()
{
    clamp_top();
    m_bar->value(m_top, visible_rows(), 0, std::max((int) m_rows.size(), visible_rows()));
}

void TraceList::scroll_to_row(int row)
{
    m_top = row;
    clamp_top();
    redraw();
}

TraceList::Columns TraceList::layout(int cw) const
{
    Columns c;
    const int left = x() + 10;
    const int avail = std::max(0, (w() - 20) / std::max(1, cw));
    /* Always: T-STATE 10+2, PC 7, SYMBOL 14, INSTRUCTION 25. Then as many
     * registers as fit, then BYTES, then a wider SYMBOL. */
    const int base = 12 + 7 + 14 + 25;
    int spare = std::max(0, avail - base);
    c.reg_count = std::min(7, spare / 5);
    spare -= c.reg_count * 5;
    const bool bytes = c.reg_count == 7 && spare >= 12;
    if (bytes) {
        spare -= 12;
    }
    const int sym_chars = 14 + std::min(14, spare);
    c.cycle_r = left + cw * 10;
    c.pc = c.cycle_r + cw * 2;
    c.sym = c.pc + cw * 7;
    c.sym_w = cw * (sym_chars - 1);
    c.bytes = bytes ? c.sym + cw * sym_chars : -1;
    c.instr = c.sym + cw * (sym_chars + (bytes ? 12 : 0));
    c.instr_w = cw * 24;
    c.regs = c.instr + cw * 25;
    return c;
}

void TraceList::draw()
{
    fl_push_clip(x(), y(), w(), h());
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());

    if (m_rows.empty()) {
        draw_empty();
        fl_pop_clip();
        return;
    }

    fl_font(DATA_FONT, DATA_SIZE);
    const int cw = char_width();
    const int lh = line_h();
    const int asc = text_baseline(lh);
    const Columns c = layout(cw);

    /* Column headers */
    fl_font(FL_HELVETICA, 11);
    fl_color(COLOR_DIM);
    const int hy = y() + 15;
    fl_draw("T-STATE", c.cycle_r - (int) fl_width("T-STATE"), hy);
    fl_draw("PC", c.pc, hy);
    fl_draw("SYMBOL", c.sym, hy);
    if (c.bytes >= 0) {
        fl_draw("BYTES", c.bytes, hy);
    }
    fl_draw("INSTRUCTION", c.instr, hy);
    for (int r = 0; r < c.reg_count; r++) fl_draw(REG_NAMES[r], c.regs + r * 5 * cw, hy);
    fl_color(COLOR_BORDER);
    fl_xyline(x() + 6, y() + head_h() - 2, x() + w() - 6);

    fl_font(DATA_FONT, DATA_SIZE);
    char buf[128];
    const int ry0 = y() + head_h();
    const int n = std::min((int) m_rows.size() - m_top, visible_rows() + 1);
    dbg_t* dbg = fz_debugger();
    for (int i = 0; i < n; i++) {
        const int idx = m_top + i;
        const zeal_trace_entry_t& e = m_rows[(size_t) idx];
        const zeal_trace_entry_t* older = idx + 1 < (int) m_rows.size() ? &m_rows[(size_t) idx + 1] : nullptr;
        const int ry = ry0 + i * lh;
        if (seq_of(idx) == m_selected) {
            fl_color(COLOR_SELECT);
            fl_rectf(x() + 4, ry, w() - 8, lh);
        } else if (idx == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(x() + 4, ry, w() - 8, lh);
        }
        const bool instr_row = e.kind == ZEAL_TRACE_INSTR;
        if (instr_row && debugger_has_breakpoint(dbg, e.pc)) {
            fl_color(COLOR_BREAK);
            fl_rectf(x() + 4, ry, 3, lh);
        }

        snprintf(buf, sizeof(buf), "%" PRIu64, e.cycle);
        fl_color(COLOR_DIM);
        fl_draw(buf, c.cycle_r - (int) fl_width(buf), ry + asc);

        if (e.kind != ZEAL_TRACE_RESET && e.kind != ZEAL_TRACE_GAP) {
            char h[8];
            hex16(h, sizeof(h), e.pc);
            snprintf(buf, sizeof(buf), "$%s", h);
            fl_color(COLOR_CYAN);
            fl_draw(buf, c.pc, ry + asc);
            m_syms.describe(buf, sizeof(buf), e.pc);
            fl_color(COLOR_AMBER);
            fl_push_clip(c.sym, ry, c.sym_w, lh);
            fl_draw(buf, c.sym, ry + asc);
            fl_pop_clip();
        }

        if (instr_row) {
            dbg_instr_t instr;
            const int size = fz_trace_disassemble(&e, &instr);
            if (c.bytes >= 0) {
                bytes_text(buf, sizeof(buf), e, size);
                fl_color(COLOR_DIM);
                fl_draw(buf, c.bytes, ry + asc);
            }
            fl_color(COLOR_TEXT);
            fl_push_clip(c.instr, ry, c.instr_w, lh);
            fl_draw(instr.instruction, c.instr, ry + asc);
            fl_pop_clip();
        } else {
            event_text(buf, sizeof(buf), e, m_syms);
            fl_color(event_color(e.kind));
            const int ex = c.bytes >= 0 ? c.bytes : c.instr;
            fl_push_clip(ex, ry, c.regs - ex - cw, lh);
            fl_draw(buf, ex, ry + asc);
            fl_pop_clip();
        }

        /* Registers before the instruction; pink when the previous one changed them */
        for (int r = 0; r < c.reg_count; r++) {
            const uint16_t v = reg_value(e, r);
            hex16(buf, sizeof(buf), v);
            const bool changed = older && older->kind == ZEAL_TRACE_INSTR && reg_value(*older, r) != v;
            fl_color(changed ? COLOR_CHANGED : COLOR_TEXT);
            fl_draw(buf, c.regs + r * 5 * cw, ry + asc);
        }
    }
    fl_pop_clip();
}

int TraceList::handle(int event)
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
            m_selected = seq_of(idx);
            redraw();
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                context_menu(idx);
            } else {
                const zeal_trace_entry_t& e = m_rows[(size_t) idx];
                if (e.kind != ZEAL_TRACE_RESET && e.kind != ZEAL_TRACE_GAP) {
                    goto_disassembly(primary_address(e));
                }
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

void TraceList::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    sync_bar();
}

void TraceList::draw_empty()
{
    fl_font(FL_HELVETICA, 13);
    fl_color(COLOR_TEXT);
    fl_draw(empty_title, x() + 16, y() + 16, w() - 32, 22, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_CLIP);
    fl_font(FL_HELVETICA, 11);
    fl_color(COLOR_DIM);
    fl_draw(empty_hint, x() + 16, y() + 42, w() - 32, h() - 50,
            FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP | FL_ALIGN_CLIP);
}

void TraceList::set_hover(int idx)
{
    if (idx == m_hover) {
        return;
    }
    m_hover = idx;
    redraw();
}

int TraceList::row_at(int ey) const
{
    const int ry0 = y() + head_h();
    if (ey < ry0) {
        return -1;
    }
    const int idx = m_top + (ey - ry0) / line_h();
    return idx < (int) m_rows.size() ? idx : -1;
}

hwaddr TraceList::primary_address(const zeal_trace_entry_t& e)
{
    return e.kind == ZEAL_TRACE_INSTR ? e.pc : e.target;
}

void TraceList::context_menu(int idx)
{
    const zeal_trace_entry_t e = m_rows[(size_t) idx];
    const bool located = e.kind != ZEAL_TRACE_RESET && e.kind != ZEAL_TRACE_GAP;
    const bool event = e.kind != ZEAL_TRACE_INSTR;
    const int off = located ? 0 : FL_MENU_INACTIVE;
    dbg_t* dbg = fz_debugger();
    const bool has_bp = debugger_has_breakpoint(dbg, e.pc);
    Fl_Menu_Item items[] = {
        { event ? "Show Handler in Disassembly" : "Show in Disassembly", 0, nullptr, nullptr, off, 0, 0, 0, 0 },
        { "Show Interrupted Address in Disassembly", 0, nullptr, nullptr, located && event ? 0 : FL_MENU_INVISIBLE, 0, 0, 0, 0 },
        { "Show in Memory", 0, nullptr, nullptr, off, 0, 0, 0, 0 },
        { "Show SP in Memory", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { has_bp ? "Remove Breakpoint" : "Add Breakpoint", 0, nullptr, nullptr, FL_MENU_DIVIDER | off, 0, 0, 0, 0 },
        { "Copy Line", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Copy Address", 0, nullptr, nullptr, off, 0, 0, 0, 0 },
        { "Copy Visible Lines", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Save Trace...", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) {
        return;
    }
    char text[8];
    std::string s;
    switch ((int) (picked - items)) {
        case 0: goto_disassembly(primary_address(e)); break;
        case 1: goto_disassembly(e.pc); break;
        case 2: goto_memory(e.pc); break;
        case 3: goto_memory(e.sp); break;
        case 4: debugger_toggle_breakpoint(dbg, e.pc); break;
        case 5:
            s = trace_line(e, m_syms);
            Fl::copy(s.c_str(), (int) s.size(), 1);
            break;
        case 6:
            hex16(text, sizeof(text), e.pc);
            Fl::copy(text, (int) strlen(text), 1);
            break;
        case 7: {
            /* Oldest first, like a saved trace */
            s = trace_header() + "\n";
            const int last = std::min((int) m_rows.size(), m_top + visible_rows()) - 1;
            for (int i = last; i >= m_top; i--) s += trace_line(m_rows[(size_t) i], m_syms) + "\n";
            Fl::copy(s.c_str(), (int) s.size(), 1);
            break;
        }
        case 8: save_trace_dialog(m_rows, m_syms); break;
        default: break;
    }
    request_refresh();
}

} // namespace zui::trace
