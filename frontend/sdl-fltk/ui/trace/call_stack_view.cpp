/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "trace/call_stack_view.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "frontend/sdl-fltk/bridge/trace.h"
#include "theme/theme.h"
#include "tools/tool_registry.h"
#include "widgets/panel.h"

namespace zui::trace {

using namespace zui::common;
using namespace zui::debugger;
using namespace zui::theme;
using namespace zui::widgets;

namespace {

constexpr int HEAD_H = 22;

widgets::Panel* make_callstack(int x, int y, int w, int h)
{
    return new CallStackView(x, y, w, h);
}

tools::ToolViewRegistrar s_reg(tools::TOOL_CODE, 40, "Call Stack", make_callstack);

const char* frame_kind(uint8_t kind)
{
    switch (kind) {
        case ZEAL_FRAME_RST: return "RST";
        case ZEAL_FRAME_INT: return "IRQ";
        case ZEAL_FRAME_NMI: return "NMI";
        default: return "CALL";
    }
}

void addr_text(char* out, size_t size, unsigned addr)
{
    char h[8];
    hex16(h, sizeof(h), addr);
    snprintf(out, size, "$%s", h);
}

struct Cols {
    int num, loc, addr, called, ret, sp;
};

Cols columns(int x, int w, int cw)
{
    Cols c;
    const int left = x + 10;
    const int chars = std::max(40, (w - 20) / std::max(1, cw));
    /* # 4 | LOCATION flex | ADDRESS 7 | CALLED flex | RETURN 7 | SP 6 */
    const int flex = chars - 4 - 7 - 7 - 6;
    const int loc = std::max(10, flex / 2);
    c.num = left;
    c.loc = left + cw * 4;
    c.addr = c.loc + cw * loc;
    c.called = c.addr + cw * 7;
    c.ret = c.called + cw * std::max(10, flex - loc);
    c.sp = c.ret + cw * 7;
    return c;
}

} // namespace

CallStackView::CallStackView(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "CALL STACK")
{
    resizable(nullptr);
    end();
    tooltip("Innermost first: where the CPU is, then each call site\n"
            "Derived from CALL/RST/interrupts and SP, so code that rewrites return addresses can fool it\n"
            "Click: show in disassembly    Right-click: more");
}

void CallStackView::refresh(bool paused, bool force)
{
    (void) paused;
    (void) force;
    m_syms.update();
    m_rows.clear();
    if (!dbg_ready()) {
        extra("");
        redraw();
        return;
    }
    zeal_frame_t frames[ZEAL_CALLSTACK_DEPTH];
    const int n = fz_callstack(frames, ZEAL_CALLSTACK_DEPTH);
    m_truncated = fz_callstack_truncated();
    /* The live PC matches the live call stack (both read between steps) */
    Row cur = {};
    cur.addr = exact_regs().pc;
    cur.current = true;
    m_rows.push_back(cur);
    for (int i = n - 1; i >= 0; i--) {
        Row r = {};
        r.addr = frames[i].call_pc;
        r.frame = frames[i];
        m_rows.push_back(r);
    }
    char text[48];
    if (fz_trace_capacity() == 0) snprintf(text, sizeof(text), "COMPILED OUT");
    else if (!fz_trace_enabled()) snprintf(text, sizeof(text), "TRACE STOPPED");
    else snprintf(text, sizeof(text), "%d FRAME%s%s", n, n == 1 ? "" : "S", m_truncated ? "+" : "");
    extra(text);
    clamp_first();
    redraw();
}

int CallStackView::rows_visible() const
{
    return std::max(1, (h() - HEADER_H - HEAD_H - 4) / data_line_height());
}

void CallStackView::clamp_first()
{
    const int total = (int) m_rows.size() + (m_truncated ? 1 : 0);
    m_first = std::max(0, std::min(m_first, total - rows_visible()));
}

void CallStackView::draw()
{
    Panel::draw();
    if (!dbg_ready()) return;
    fl_push_clip(x(), top(), w(), h() - HEADER_H);
    fl_font(DATA_FONT, DATA_SIZE);
    const int cw = char_width();
    const int lh = data_line_height();
    const int asc = text_baseline(lh);
    const Cols c = columns(x(), w(), cw);

    fl_font(FL_HELVETICA, 11);
    fl_color(COLOR_DIM);
    const int hy = top() + 15;
    fl_draw("#", c.num, hy);
    fl_draw("LOCATION", c.loc, hy);
    fl_draw("ADDRESS", c.addr, hy);
    fl_draw("CALLED", c.called, hy);
    fl_draw("RETURN", c.ret, hy);
    fl_draw("SP", c.sp, hy);
    fl_color(COLOR_BORDER);
    fl_xyline(x() + 6, top() + HEAD_H - 2, x() + w() - 6);

    fl_font(DATA_FONT, DATA_SIZE);
    const int ry0 = top() + HEAD_H;
    const int vis = rows_visible();
    for (int i = 0; i < vis + 1; i++) {
        const int idx = m_first + i;
        const int ry = ry0 + i * lh;
        if (idx < (int) m_rows.size()) {
            draw_row(m_rows[(size_t) idx], idx, ry, lh, asc, cw);
        } else if (idx == (int) m_rows.size() && m_truncated) {
            fl_font(FL_HELVETICA, 11);
            fl_color(COLOR_DIM);
            fl_draw("... outer frames unknown (stack overflowed, or tracing started inside a call)",
                    c.loc, ry, w() - (c.loc - x()) - 10, lh, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
            fl_font(DATA_FONT, DATA_SIZE);
        }
    }
    if (fz_trace_capacity() == 0 || !fz_trace_enabled()) {
        fl_font(FL_HELVETICA, 11);
        fl_color(COLOR_DIM);
        fl_draw(fz_trace_capacity() == 0 ? "The call stack needs the trace (built with -Dtrace_depth=0)."
                                         : "The call stack is only tracked while the Trace is recording.",
                x() + 10, ry0 + lh + 4, w() - 20, lh * 2, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP | FL_ALIGN_CLIP);
    }
    fl_pop_clip();
}

void CallStackView::draw_row(const Row& row, int index, int ry, int lh, int asc, int cw)
{
    const Cols c = columns(x(), w(), cw);
    if (index == m_selected) {
        fl_color(COLOR_SELECT);
        fl_rectf(x() + 4, ry, w() - 8, lh);
    } else if (index == m_hover) {
        fl_color(COLOR_HOVER);
        fl_rectf(x() + 4, ry, w() - 8, lh);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "%d", index);
    fl_color(row.current ? COLOR_AMBER : COLOR_DIM);
    fl_draw(row.current ? ">" : buf, c.num, ry + asc);

    m_syms.describe(buf, sizeof(buf), row.addr);
    fl_color(COLOR_AMBER);
    fl_push_clip(c.loc, ry, c.addr - c.loc - cw, lh);
    fl_draw(buf[0] ? buf : "?", c.loc, ry + asc);
    fl_pop_clip();

    addr_text(buf, sizeof(buf), row.addr);
    fl_color(COLOR_CYAN);
    fl_draw(buf, c.addr, ry + asc);

    if (row.current) {
        fl_font(FL_HELVETICA, 11);
        fl_color(COLOR_DIM);
        fl_draw("current PC", c.called, ry, c.ret - c.called, lh, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
        fl_font(DATA_FONT, DATA_SIZE);
        return;
    }
    char sym[96];
    m_syms.describe(sym, sizeof(sym), row.frame.target);
    if (sym[0] == 0) addr_text(sym, sizeof(sym), row.frame.target);
    snprintf(buf, sizeof(buf), "%-4s %s", frame_kind(row.frame.kind), sym);
    fl_color(row.frame.kind == ZEAL_FRAME_INT || row.frame.kind == ZEAL_FRAME_NMI ? COLOR_GREEN : COLOR_TEXT);
    fl_push_clip(c.called, ry, c.ret - c.called - cw, lh);
    fl_draw(buf, c.called, ry + asc);
    fl_pop_clip();

    addr_text(buf, sizeof(buf), row.frame.ret);
    fl_color(COLOR_CYAN);
    fl_draw(buf, c.ret, ry + asc);
    char h[8];
    hex16(h, sizeof(h), row.frame.sp);
    fl_color(COLOR_DIM);
    fl_draw(h, c.sp, ry + asc);
}

int CallStackView::row_at(int ey) const
{
    const int ry0 = top() + HEAD_H;
    if (ey < ry0) return -1;
    const int idx = m_first + (ey - ry0) / data_line_height();
    return idx < (int) m_rows.size() ? idx : -1;
}

int CallStackView::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            m_hover = -1;
            redraw();
            return 1;
        case FL_MOVE: {
            const int r = row_at(Fl::event_y());
            if (r != m_hover) {
                m_hover = r;
                redraw();
            }
            return 1;
        }
        case FL_MOUSEWHEEL:
            m_first += Fl::event_dy() * 2;
            clamp_first();
            redraw();
            return 1;
        case FL_PUSH: {
            const int r = row_at(Fl::event_y());
            if (r < 0 || !dbg_ready()) return 1;
            m_selected = r;
            redraw();
            if (Fl::event_button() == FL_RIGHT_MOUSE) context_menu(r);
            else goto_disassembly(m_rows[(size_t) r].addr);
            return 1;
        }
        default:
            break;
    }
    return Fl_Group::handle(event);
}

std::string CallStackView::row_text(const Row& row, int index) const
{
    char loc[96], addr[8], line[256];
    m_syms.describe(loc, sizeof(loc), row.addr);
    hex16(addr, sizeof(addr), row.addr);
    if (row.current) {
        snprintf(line, sizeof(line), "#%-2d $%s  %s", index, addr, loc);
        return line;
    }
    char target[8], ret[8], sp[8], sym[96];
    hex16(target, sizeof(target), row.frame.target);
    hex16(ret, sizeof(ret), row.frame.ret);
    hex16(sp, sizeof(sp), row.frame.sp);
    m_syms.describe(sym, sizeof(sym), row.frame.target);
    snprintf(line, sizeof(line), "#%-2d $%s  %-24s %-4s $%s %-24s ret $%s  sp $%s", index, addr, loc,
             frame_kind(row.frame.kind), target, sym, ret, sp);
    return line;
}

void CallStackView::context_menu(int idx)
{
    const Row row = m_rows[(size_t) idx];
    const int frame_only = row.current ? FL_MENU_INACTIVE : 0;
    Fl_Menu_Item items[] = {
        { "Show in Disassembly", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Show Called Address in Disassembly", 0, nullptr, nullptr, frame_only, 0, 0, 0, 0 },
        { "Show Return Address in Disassembly", 0, nullptr, nullptr, frame_only, 0, 0, 0, 0 },
        { "Show Stack Slot in Memory", 0, nullptr, nullptr, FL_MENU_DIVIDER | frame_only, 0, 0, 0, 0 },
        { "Copy Address", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Copy Call Stack", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) return;
    char text[8];
    std::string s;
    switch ((int) (picked - items)) {
        case 0: goto_disassembly(row.addr); break;
        case 1: goto_disassembly(row.frame.target); break;
        case 2: goto_disassembly(row.frame.ret); break;
        case 3: goto_memory(row.frame.sp); break;
        case 4:
            hex16(text, sizeof(text), row.addr);
            Fl::copy(text, (int) strlen(text), 1);
            break;
        case 5:
            for (size_t i = 0; i < m_rows.size(); i++) s += row_text(m_rows[i], (int) i) + "\n";
            if (m_truncated) s += "...\n";
            Fl::copy(s.c_str(), (int) s.size(), 1);
            break;
        default: break;
    }
}

} // namespace zui::trace
