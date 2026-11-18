/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/registers_view.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "frontend/sdl-fltk/bridge/cpu.h"
#include "theme/style.h"
#include "theme/theme.h"
#include "widgets/inline_edit.h"
#include "widgets/panel.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;
using namespace zui::widgets;

namespace {

const char* s_reg_names[R_COUNT] = {
    "AF", "BC", "DE", "HL", "IX", "SP", "I",
    "AF'", "BC'", "DE'", "HL'", "IY", "PC", "R",
};

bool reg_is_8bit(int id) { return id == R_I || id == R_R; }

uint16_t reg_get(const regs_t& r, int id)
{
    switch (id) {
        case R_AF: return r.af;   case R_BC: return r.bc;
        case R_DE: return r.de;   case R_HL: return r.hl;
        case R_IX: return r.ix;   case R_IY: return r.iy;
        case R_SP: return r.sp;   case R_PC: return r.pc;
        case R_AF_: return r.af_; case R_BC_: return r.bc_;
        case R_DE_: return r.de_; case R_HL_: return r.hl_;
        case R_I: return r.i;     case R_R: return r.r;
        default: return 0;
    }
}

void reg_set(regs_t& r, int id, uint16_t v)
{
    switch (id) {
        case R_AF: r.af = v;   break; case R_BC: r.bc = v;   break;
        case R_DE: r.de = v;   break; case R_HL: r.hl = v;   break;
        case R_IX: r.ix = v;   break; case R_IY: r.iy = v;   break;
        case R_SP: r.sp = v;   break; case R_PC: r.pc = v;   break;
        case R_AF_: r.af_ = v; break; case R_BC_: r.bc_ = v; break;
        case R_DE_: r.de_ = v; break; case R_HL_: r.hl_ = v; break;
        case R_I: r.i = (uint8_t) v; break;
        case R_R: r.r = (uint8_t) v; break;
        default: break;
    }
}

} // namespace

RegistersView::RegistersView(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "CPU")
{
    m_edit = new InlineEdit(X, Y, 10, 10);
    style_input(m_edit);
    m_edit->box(FL_FLAT_BOX);
    m_edit->color(COLOR_SELECT);
    m_edit->when(FL_WHEN_ENTER_KEY_ALWAYS);
    m_edit->callback(cb_commit, this);
    m_edit->hide();
    resizable(nullptr);
    end();
    header_toggle("MORE", m_more, [this](bool on) { set_more(on); });
}

void RegistersView::set_more(bool more)
{
    m_more = more;
    header_toggle("MORE", m_more, [this](bool on) { set_more(on); });
    if (on_layout) on_layout();
    redraw();
}

int RegistersView::preferred_height() const
{
    const int lh = data_line_height();
    /* registers, flag chips; then interrupt chips, (HL)/(SP), timing */
    const int basic = HEADER_H + 8 + 7 * lh + 22 + (lh + 4) + 10;
    if (!m_more) return basic;
    return basic - 10 + 18 + (lh + 4) + 12 + 2 * lh + TIMING_GAP + 2 * lh + 8;
}

void RegistersView::refresh(bool paused, bool force)
{
    m_regs = current_regs();
    m_paused = paused;
    for (int id = 0; id < R_COUNT; id++) {
        const uint16_t v = reg_get(m_regs, id);
        if (paused && force) {
            m_changed[id] = m_have_prev && v != m_prev[id];
            m_prev[id] = v;
        } else if (!paused) {
            m_changed[id] = false;
        }
    }
    if (paused && force) m_have_prev = true;
    if (dbg_ready()) {
        display_memory(m_regs.hl, 8, m_peek_hl, false);
        display_memory(m_regs.sp, 2, m_peek_sp, true);
    }
    /* Interrupt state and raster position belong with the registers (the
     * sample while running); the counters are read live. */
    fz_cpu_state_t live;
    m_have_cpu = fz_cpu_state(&live);
    if (m_have_cpu) {
        m_cpu = live;
        fz_cpu_state_t sample;
        if (!paused && fz_cpu_state_sampled(&sample)) {
            sample.tstates = live.tstates;
            sample.elapsed_us = live.elapsed_us;
            sample.frame = live.frame;
            m_cpu = sample;
        }
    }
    extra(paused ? "PAUSED" : "SAMPLED");
    redraw();
}

void RegistersView::draw()
{
    if (content_hidden()) {
        Panel::draw();
        return;
    }
    /* The stack may give this panel less than its preferred height */
    fl_push_clip(x(), y(), w(), h());
    draw_contents();
    fl_pop_clip();
}

void RegistersView::draw_contents()
{
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    draw_panel_header();
    const int lh = data_line_height();
    const int cw = char_width();
    const int asc = text_baseline(lh);
    char text[48];

    fl_font(DATA_FONT, DATA_SIZE);
    for (int id = 0; id < R_COUNT; id++) {
        int nx, vx, ry;
        cell(id, nx, vx, ry);
        fl_color(reg_is_8bit(id) ? COLOR_DIM : COLOR_CYAN);
        fl_draw(s_reg_names[id], nx, ry + asc);
        if (reg_is_8bit(id)) hex8(text, sizeof(text), reg_get(m_regs, id));
        else hex16(text, sizeof(text), reg_get(m_regs, id));
        if (id == m_hover && !reg_is_8bit(id)) {
            fl_color(COLOR_HOVER);
            fl_rectf(vx - 3, ry, cw * 4 + 6, lh);
        }
        fl_color(m_changed[id] ? COLOR_CHANGED : COLOR_TEXT);
        fl_draw(text, vx, ry + asc);
    }

    /* Flags as chips: F register bit 7..0 = S Z 5 H 3 P/V N C */
    static const char* names[8] = { "S", "Z", "5", "H", "3", "P", "N", "C" };
    const int fy = flags_y();
    fl_color(COLOR_DIM);
    fl_font(FL_HELVETICA, 10);
    fl_draw("FLAGS", x() + 10, fy - 5);
    fl_font(DATA_FONT, DATA_SIZE);
    for (int i = 0; i < 8; i++) {
        int cx, cwid;
        chip(i, cx, cwid);
        const int bit = 7 - i;
        const bool on = (m_regs.f >> bit) & 1;
        const bool undocumented = bit == 5 || bit == 3;
        if (on) {
            fl_color(undocumented ? COLOR_DIM : COLOR_GREEN);
            fl_rectf(cx, fy, cwid, lh + 4);
            fl_color(COLOR_BG);
        } else {
            fl_color(COLOR_BORDER);
            fl_rect(cx, fy, cwid, lh + 4);
            fl_color(undocumented ? COLOR_BORDER : COLOR_DIM);
        }
        fl_draw(names[i], cx + (cwid - (int) fl_width(names[i])) / 2, fy + 2 + asc);
    }

    if (!m_more) {
        if (m_edit->visible()) draw_child(*m_edit);
        return;
    }

    /* Interrupt state: IM value chip, then lit/unlit state chips */
    const int iy = int_y();
    fl_color(COLOR_DIM);
    fl_font(FL_HELVETICA, 10);
    fl_draw("INTERRUPTS", x() + 10, iy - 5);
    fl_font(DATA_FONT, DATA_SIZE);
    fl_push_clip(x(), iy, w() - 4, lh + 4);
    for (int i = 0; i < INT_CHIPS; i++) {
        int cx, cwid;
        int_chip(i, cx, cwid);
        char label[8];
        Fl_Color fill = 0;
        bool on = false;
        switch (i) {
            case 0: snprintf(label, sizeof(label), "IM %u", (unsigned) m_cpu.im); break;
            case 1: snprintf(label, sizeof(label), "IFF1"); on = m_cpu.iff1; fill = COLOR_GREEN; break;
            case 2: snprintf(label, sizeof(label), "IFF2"); on = m_cpu.iff2; fill = COLOR_GREEN; break;
            case 3: snprintf(label, sizeof(label), "HALT"); on = m_cpu.halted; fill = COLOR_AMBER; break;
            case 4: snprintf(label, sizeof(label), "INT"); on = m_cpu.int_pending; fill = COLOR_AMBER; break;
            default: snprintf(label, sizeof(label), "NMI"); on = m_cpu.nmi_pending; fill = COLOR_BREAK; break;
        }
        if (i == 0) {
            fl_color(COLOR_RAISED);
            fl_rectf(cx, iy, cwid, lh + 4);
            fl_color(COLOR_BORDER);
            fl_rect(cx, iy, cwid, lh + 4);
            fl_color(m_have_cpu ? COLOR_AMBER : COLOR_DIM);
        } else if (on) {
            fl_color(fill);
            fl_rectf(cx, iy, cwid, lh + 4);
            fl_color(COLOR_BG);
        } else {
            fl_color(COLOR_BORDER);
            fl_rect(cx, iy, cwid, lh + 4);
            fl_color(COLOR_DIM);
        }
        fl_draw(label, cx + (cwid - (int) fl_width(label)) / 2, iy + 2 + asc);
    }
    fl_pop_clip();

    /* What HL and SP point at */
    int py = iy + lh + 4 + 12;
    fl_color(COLOR_CYAN);
    fl_draw("(HL)", x() + 10, py + asc);
    int n = 0;
    int fit = (w() - 20 - cw * 5) / (cw * 3);
    if (fit > 8) fit = 8;
    for (int i = 0; i < fit && n < (int) sizeof(text) - 4; i++) {
        n += snprintf(text + n, sizeof(text) - n, hex8_fmt(), m_peek_hl[i]);
        text[n++] = ' ';
        text[n] = '\0';
    }
    fl_color(COLOR_TEXT);
    fl_draw(text, x() + 10 + cw * 5, py + asc);
    py += lh;
    const unsigned top_word = (unsigned) (m_peek_sp[0] | (m_peek_sp[1] << 8));
    const bool sp_word = m_regs.sp <= 0xFFFE; /* no stack word above $FFFE */
    fl_color(COLOR_CYAN);
    fl_draw("(SP)", x() + 10, py + asc);
    if (sp_word) hex16(text, sizeof(text), top_word); else snprintf(text, sizeof(text), "----");
    fl_color(sp_word ? COLOR_TEXT : COLOR_DIM);
    fl_draw(text, x() + 10 + cw * 5, py + asc);
    const char* sym = (dbg_ready() && sp_word) ? debugger_get_symbol(fz_debugger(), top_word) : nullptr;
    if (sym) {
        fl_color(COLOR_AMBER);
        fl_draw(sym, x() + 10 + cw * 10, py + asc);
    }

    draw_timing(lh, cw, asc);

    if (m_edit->visible()) draw_child(*m_edit);
}

int RegistersView::handle(int event)
{
    if (content_hidden() || (event == FL_PUSH && Fl::event_y() < y() + HEADER_H)) return Panel::handle(event);
    if (m_edit->visible() && (event == FL_KEYBOARD || event == FL_KEYUP || event == FL_SHORTCUT)) {
        return Fl_Group::handle(event);
    }
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            m_hover = -1;
            redraw();
            return 1;
        case FL_MOVE: {
            int id = hit_value(Fl::event_x(), Fl::event_y());
            if (id < 0) id = hit_name(Fl::event_x(), Fl::event_y());
            if (id < 0) {
                const int c = hit_int_chip(Fl::event_x(), Fl::event_y());
                if (c >= 0) id = HOVER_INT + c;
                else if (hit_timing(Fl::event_y())) id = HOVER_TIMING;
            }
            if (id != m_hover) {
                m_hover = id;
                redraw();
                if (id >= HOVER_INT) tooltip_extra(id);
                else tooltip_for(id);
            }
            return 1;
        }
        case FL_MOUSEWHEEL:
            return 1;
        case FL_PUSH: {
            if (m_edit->visible() && Fl::event_inside(m_edit)) break;
            const int ex = Fl::event_x(), ey = Fl::event_y();
            int id = hit_name(ex, ey);
            if (id < 0) id = hit_value(ex, ey);
            if (id >= 0) {
                if (Fl::event_button() == FL_RIGHT_MOUSE) {
                    context_menu(id);
                } else if (!reg_is_8bit(id)) {
                    goto_memory(reg_get(m_regs, id));
                }
                return 1;
            }
            const int f = hit_chip(ex, ey);
            if (f >= 0 && m_paused && dbg_ready()) {
                regs_t regs = exact_regs();
                regs.f ^= (uint8_t) (1u << (7 - f));
                debugger_set_registers(fz_debugger(), &regs);
                request_refresh();
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Group::handle(event);
}

void RegistersView::cell(int id, int& name_x, int& value_x, int& row_y) const
{
    const int lh = data_line_height();
    const int cw = char_width();
    const int col = id / 7;
    const int row = id % 7;
    name_x = x() + 10 + col * (w() / 2);
    value_x = name_x + cw * 4;
    row_y = y() + HEADER_H + 8 + row * lh;
}

int RegistersView::flags_y() const
{
    return y() + HEADER_H + 8 + 7 * data_line_height() + 22;
}

void RegistersView::chip(int i, int& cx, int& cw) const
{
    cw = (w() - 20) / 8 - 4;
    if (cw > 30) cw = 30;
    cx = x() + 10 + i * (cw + 4);
}

int RegistersView::int_y() const
{ return flags_y() + data_line_height() + 4 + 18; }

int RegistersView::timing_y() const
{ return int_y() + 3 * data_line_height() + 4 + 12 + TIMING_GAP; }

void RegistersView::int_chip(int i, int& cx, int& cw) const
{
    /* Widths fit the labels ("IM 1", four-letter chips, then INT/NMI);
     * the padding shrinks so all six fit a narrow column. */
    const int c = char_width();
    static const int chars[INT_CHIPS] = { 4, 4, 4, 4, 3, 3 };
    constexpr int gap = 4;
    int total = 0;
    for (int k = 0; k < INT_CHIPS; k++) total += chars[k] * c;
    int pad = (w() - 20 - total - gap * (INT_CHIPS - 1)) / INT_CHIPS;
    if (pad > 10) pad = 10;
    if (pad < 2) pad = 2;
    cx = x() + 10;
    for (int k = 0; k < i; k++) cx += chars[k] * c + pad + gap;
    cw = chars[i] * c + pad;
}

int RegistersView::hit_int_chip(int ex, int ey) const
{
    if (!m_more) return -1;
    const int iy = int_y();
    if (ey < iy || ey >= iy + data_line_height() + 4) return -1;
    for (int i = 0; i < INT_CHIPS; i++) {
        int cx, cw;
        int_chip(i, cx, cw);
        if (ex >= cx && ex < cx + cw) return i;
    }
    return -1;
}

bool RegistersView::hit_timing(int ey) const
{
    if (!m_more) return false;
    const int ty = timing_y();
    return ey >= ty && ey < ty + 2 * data_line_height();
}

void RegistersView::group_digits(char* out, size_t size, uint64_t value)
{
    char digits[32];
    snprintf(digits, sizeof(digits), "%llu", (unsigned long long) value);
    const int len = (int) strlen(digits);
    size_t n = 0;
    for (int i = 0; i < len && n + 2 < size; i++) {
        if (i > 0 && (len - i) % 3 == 0) out[n++] = ',';
        out[n++] = digits[i];
    }
    out[n] = '\0';
}

void RegistersView::draw_timing(int lh, int cw, int asc)
{
    const int ty = timing_y();
    fl_color(COLOR_BORDER);
    fl_xyline(x() + 10, ty - TIMING_GAP / 2 - 1, x() + w() - 10);
    if (!m_have_cpu) return;
    char text[40];
    const int lx = x() + 10;
    const int vx = lx + cw * 7;
    fl_font(DATA_FONT, DATA_SIZE);

    fl_color(COLOR_DIM);
    fl_draw("CYCLES", lx, ty + asc);
    group_digits(text, sizeof(text), m_cpu.tstates);
    fl_color(COLOR_TEXT);
    fl_draw(text, vx, ty + asc);
    const int used = vx + (int) fl_width(text);
    const double secs = (double) m_cpu.elapsed_us / 1e6;
    snprintf(text, sizeof(text), secs < 1000.0 ? "%.3fs" : "%.0fs", secs);
    const int tw = (int) fl_width(text);
    if (used + cw * 2 + tw <= x() + w() - 10) {
        fl_color(COLOR_DIM);
        fl_draw(text, x() + w() - 10 - tw, ty + asc);
    }

    const int y2 = ty + lh;
    fl_color(COLOR_DIM);
    fl_draw("FRAME", lx, y2 + asc);
    group_digits(text, sizeof(text), m_cpu.frame);
    fl_color(COLOR_TEXT);
    fl_draw(text, vx, y2 + asc);

    /* Raster in the right column, aligned with the second register column.
     * Values turn amber inside the blanking interval. */
    const int rx = x() + 10 + w() / 2;
    fl_color(COLOR_DIM);
    fl_draw("Y", rx, y2 + asc);
    snprintf(text, sizeof(text), "%3d", m_cpu.scanline);
    fl_color(m_cpu.vblank ? COLOR_AMBER : COLOR_TEXT);
    fl_draw(text, rx + cw * 2, y2 + asc);
    fl_color(COLOR_DIM);
    fl_draw("X", rx + cw * 6, y2 + asc);
    snprintf(text, sizeof(text), "%3d", m_cpu.hpos);
    fl_color(m_cpu.hpos >= m_cpu.visible_pixels ? COLOR_AMBER : COLOR_TEXT);
    fl_draw(text, rx + cw * 8, y2 + asc);
}

void RegistersView::tooltip_extra(int hover)
{
    static char tip[320];
    const int lh = data_line_height();
    if (hover >= HOVER_INT && hover < HOVER_INT + INT_CHIPS) {
        static const char* tips[INT_CHIPS] = {
            "Interrupt mode (IM 0 / IM 1 / IM 2)",
            "IFF1: maskable interrupts enabled (EI / DI)",
            "IFF2: copy of IFF1 saved during NMI, restored by RETN",
            "HALT: the CPU is halted, waiting for an interrupt",
            "INT: a maskable interrupt is pending (not yet accepted)",
            "NMI: a non-maskable interrupt is pending",
        };
        int cx, cw;
        int_chip(hover - HOVER_INT, cx, cw);
        Fl_Tooltip::enter_area(this, cx - x(), int_y() - y(), cw, lh + 4, tips[hover - HOVER_INT]);
    } else if (hover == HOVER_TIMING) {
        snprintf(tip, sizeof(tip),
                 "CYCLES: CPU T-states since reset (live), with emulated time\n"
                 "FRAME: video frames since reset (live)\n"
                 "Y / X: raster scanline and pixel (decimal)%s\n"
                 "  Y 0-%d visible, %d-%d vertical blank\n"
                 "  X 0-%d visible, %d-%d horizontal blank\n"
                 "Amber: inside the blanking interval",
                 m_paused ? "" : ", at the sample point",
                 m_cpu.visible_lines - 1, m_cpu.visible_lines, m_cpu.total_lines - 1,
                 m_cpu.visible_pixels - 1, m_cpu.visible_pixels, m_cpu.line_pixels - 1);
        Fl_Tooltip::enter_area(this, 0, timing_y() - y(), w(), 2 * lh, tip);
    }
}

int RegistersView::hit_value(int ex, int ey) const
{
    const int lh = data_line_height();
    const int cw = char_width();
    for (int id = 0; id < R_COUNT; id++) {
        int nx, vx, ry;
        cell(id, nx, vx, ry);
        if (ex >= vx - 3 && ex < vx + cw * 4 + 3 && ey >= ry && ey < ry + lh) return id;
    }
    return -1;
}

int RegistersView::hit_name(int ex, int ey) const
{
    const int lh = data_line_height();
    const int cw = char_width();
    for (int id = 0; id < R_COUNT; id++) {
        int nx, vx, ry;
        cell(id, nx, vx, ry);
        if (ex >= nx && ex < nx + cw * 3 && ey >= ry && ey < ry + lh) return id;
    }
    return -1;
}

int RegistersView::hit_chip(int ex, int ey) const
{
    const int fy = flags_y();
    if (ey < fy || ey >= fy + data_line_height() + 4) return -1;
    for (int i = 0; i < 8; i++) {
        int cx, cw;
        chip(i, cx, cw);
        if (ex >= cx && ex < cx + cw) return i;
    }
    return -1;
}

void RegistersView::tooltip_for(int id)
{
    static char tip[128];
    if (id < 0) return;
    const unsigned v = reg_get(m_regs, id);
    if (reg_is_8bit(id)) {
        snprintf(tip, sizeof(tip), "%s = %u\nRight-click: edit", s_reg_names[id], v);
    } else {
        const int hi = (v >> 8) & 0xFF, lo = v & 0xFF;
        snprintf(tip, sizeof(tip), "%s = %u (signed %d)\nhigh %u '%c'   low %u '%c'\n"
                 "Click: show in memory    Right-click: more",
                 s_reg_names[id], v, (int) (int16_t) v, hi, isprint(hi) ? hi : '.', lo,
                 isprint(lo) ? lo : '.');
    }
    int nx, vx, ry;
    cell(id, nx, vx, ry);
    Fl_Tooltip::enter_area(this, vx - x(), ry - y(), char_width() * 4, data_line_height(), tip);
}

void RegistersView::context_menu(int id)
{
    const bool addr = !reg_is_8bit(id);
    const uint16_t value = reg_get(m_regs, id);
    const int addr_only = addr ? 0 : FL_MENU_INACTIVE;
    const int pause_only = m_paused ? 0 : FL_MENU_INACTIVE;
    Fl_Menu_Item items[] = {
        { "Show in Memory", 0, nullptr, nullptr, addr_only, 0, 0, 0, 0 },
        { "Show in Disassembly", 0, nullptr, nullptr, addr_only | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Edit Value...", 0, nullptr, nullptr, pause_only, 0, 0, 0, 0 },
        { "Copy Value", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) return;
    char text[8];
    switch ((int) (picked - items)) {
        case 0: goto_memory(value); break;
        case 1: goto_disassembly(value); break;
        case 2: begin_edit(id); break;
        case 3:
            if (addr) hex16(text, sizeof(text), value); else hex8(text, sizeof(text), value);
            Fl::copy(text, (int) strlen(text), 1);
            break;
        default: break;
    }
}

void RegistersView::begin_edit(int id)
{
    int nx, vx, ry;
    cell(id, nx, vx, ry);
    char text[8];
    if (reg_is_8bit(id)) hex8(text, sizeof(text), reg_get(m_regs, id));
    else hex16(text, sizeof(text), reg_get(m_regs, id));
    m_edit_id = id;
    m_edit->resize(vx - 3, ry, char_width() * 4 + 8, data_line_height());
    /* Hex digits, plus room for an optional '$' */
    m_edit->maximum_size(reg_is_8bit(id) ? 3 : 5);
    m_edit->value(text);
    m_edit->show();
    m_edit->take_focus();
#if FL_API_VERSION >= 10400
    m_edit->insert_position(0, (int) strlen(text));
#else
    m_edit->position(0, (int) strlen(text));
#endif
    redraw();
}

bool parse_register_text(const char* text, bool eight_bit, uint16_t* value)
{
    /* Only hex digits after an optional '$', at least one, at most the
     * register's width: an empty or bare "$" input changes nothing */
    const char* digits = text[0] == '$' ? text + 1 : text;
    const size_t n = strlen(digits);
    if (n < 1 || n > (eight_bit ? 2u : 4u)) return false;
    for (size_t i = 0; i < n; i++) {
        if (!isxdigit((unsigned char) digits[i])) return false;
    }
    *value = (uint16_t) strtoul(digits, nullptr, 16);
    return true;
}

void RegistersView::cb_commit(Fl_Widget*, void* data)
{
    RegistersView* self = static_cast<RegistersView*>(data);
    uint16_t v = 0;
    const bool valid = parse_register_text(self->m_edit->value(), reg_is_8bit(self->m_edit_id), &v);
    self->m_edit->hide();
    if (!valid || !dbg_ready() || !fz_paused()) {
        fl_beep();
    } else {
        regs_t regs = exact_regs();
        reg_set(regs, self->m_edit_id, (uint16_t) v);
        debugger_set_registers(fz_debugger(), &regs);
    }
    request_refresh();
}

} // namespace zui::debugger
