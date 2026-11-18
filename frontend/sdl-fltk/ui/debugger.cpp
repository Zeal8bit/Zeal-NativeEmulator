/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Debugger layout. Everything needed while stepping stays visible together,
 * no tabs (in the spirit of Mesen and no$sns):
 *
 *   +---------------------------------------------------------------+
 *   | toolbar: run/pause  step  over  run-to | reset          | state  |
 *   +----------------+---------------------+-----------------------+
 *   | screen         | disassembly         | CPU registers, flags  |
 *   |                |                     +-----------------------+
 *   +----------------+                     | stack (SP+n)          |
 *   | memory + MMU   |                     +-----------------------+
 *   |                |                     | breakpoints           |
 *   +----------------+---------------------+-----------------------+
 *
 * Graphics viewers and the profiler live in a separate tool window
 * (video.cpp). Split sizes are saved under FLTK_* config keys. */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <FL/Fl_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include <FL/fl_utf8.h>
#include <FL/fl_ask.H>
#include "panel.h"
#include "tool_window.h"
#include "video.h"
#include "memory_tools.h"

#if CONFIG_ENABLE_DEBUGGER
extern "C" {
#include "debugger/zeal_debugger.h"
}
#include "frontend/sdl-fltk/bridge/cpu.h"
#endif

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

static Debugger::Impl* s_impl;

/* ------------------------------------------------------------------ */
/*  Toolbar                                                            */
/* ------------------------------------------------------------------ */

class StatePill : public Fl_Widget {
public:
    StatePill(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H) {}

    void set(bool paused, uint16_t pc)
    {
        if (paused) {
            char addr[8];
            hex16(addr, sizeof(addr), pc);
            snprintf(m_text, sizeof(m_text), "PAUSED $%s", addr);
        } else {
            snprintf(m_text, sizeof(m_text), "RUNNING");
        }
        m_paused = paused;
        redraw();
    }

    void draw() override
    {
        fl_font(DATA_FONT, DATA_SIZE);
        const int tw = (int) fl_width(m_text);
        const int pw = tw + 32;
        const int px = x() + w() - pw;
        const Fl_Color c = m_paused ? COLOR_AMBER : COLOR_GREEN;
        fl_color(COLOR_BG);
        fl_rectf(x(), y(), w(), h());
        fl_color(COLOR_RAISED);
        fl_rectf(px, y() + 2, pw, h() - 4);
        fl_color(c);
        fl_rect(px, y() + 2, pw, h() - 4);
        fl_pie(px + 9, y() + h() / 2 - 4, 8, 8, 0, 360);
        fl_draw(m_text, px + 24, y() + (h() + fl_height()) / 2 - fl_descent());
    }

private:
    char m_text[32] = "RUNNING";
    bool m_paused = false;
};

class Toolbar : public Fl_Group {
public:
    Toolbar(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H)
    {
        box(FL_FLAT_BOX);
        color(COLOR_BG);
        int bx = X + 8;
        const int by = Y + 4;
        const int bh = H - 8;
        auto make = [&](Icon icon, const char* text, int w, Fl_Callback* cb, const char* tip) {
            IconButton* b = new IconButton(bx, by, w, bh, icon, text);
            b->callback(cb);
            b->tooltip(tip);
            bx += w + 4;
            return b;
        };
        m_run = make(Icon::Pause, "Pause", 96, cb_run, "Pause / continue (Cmd/Ctrl+F6, Cmd/Ctrl+F5)");
        m_run->icon_color(COLOR_GREEN);
        make(Icon::StepInto, "Step", 72, cb_step, "Step into (Cmd/Ctrl+F11)");
        make(Icon::StepOver, "Over", 72, cb_step_over, "Step over (Cmd/Ctrl+F10)");
        m_run_to = make(Icon::RunTo, "Run to", 84, cb_run_to, "Run to the line selected in the disassembly");
        bx += 12;
        make(Icon::Reset, "Reset", 76, cb_reset, "Reset the machine (Cmd/Ctrl+Shift+Backspace)");
        m_pill = new StatePill(X + W - 230, by, 222, bh);
        resizable(nullptr);
        end();
    }

    void resize(int X, int Y, int W, int H) override
    {
        const int dx = X - x();
        const int dy = Y - y();
        Fl_Widget::resize(X, Y, W, H);
        for (int i = 0; i < children(); i++) {
            Fl_Widget* c = child(i);
            if (c == m_pill) continue;
            c->resize(c->x() + dx, c->y() + dy, c->w(), c->h());
        }
        m_pill->resize(X + W - 230, Y + 4, 222, H - 8);
    }

    void update(bool paused, uint16_t pc)
    {
        m_run->icon(paused ? Icon::Play : Icon::Pause);
        m_run->text(paused ? "Continue" : "Pause");
        if (paused) m_run_to->activate(); else m_run_to->deactivate();
        m_pill->set(paused, pc);
    }

    void draw() override
    {
        Fl_Group::draw();
        fl_color(COLOR_BORDER);
        fl_xyline(x(), y() + h() - 1, x() + w() - 1);
    }

    static void cb_run_to(Fl_Widget*, void*);

private:
    static void cb_run(Fl_Widget*, void*)
    {
        if (!dbg_ready()) return;
        if (fz_paused()) debugger_continue(fz_debugger()); else debugger_pause(fz_debugger());
        request_refresh();
    }
    static void cb_step(Fl_Widget*, void*)      { if (dbg_ready()) debugger_step(fz_debugger()); }
    static void cb_step_over(Fl_Widget*, void*) { if (dbg_ready()) debugger_step_over(fz_debugger()); }
    static void cb_reset(Fl_Widget*, void*)
    {
        fz_reset();
        request_refresh();
    }

    IconButton* m_run;
    IconButton* m_run_to;
    StatePill* m_pill;
};

/* ------------------------------------------------------------------ */
/*  CPU registers                                                      */
/* ------------------------------------------------------------------ */

enum {
    R_AF, R_BC, R_DE, R_HL, R_IX, R_SP, R_I,
    R_AF_, R_BC_, R_DE_, R_HL_, R_IY, R_PC, R_R,
    R_COUNT
};

static const char* s_reg_names[R_COUNT] = {
    "AF", "BC", "DE", "HL", "IX", "SP", "I",
    "AF'", "BC'", "DE'", "HL'", "IY", "PC", "R",
};

static bool reg_is_8bit(int id) { return id == R_I || id == R_R; }

static uint16_t reg_get(const regs_t& r, int id)
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

static void reg_set(regs_t& r, int id, uint16_t v)
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

/** Inline editor that cancels on Escape or focus loss. */
class InlineEdit : public Fl_Input {
public:
    InlineEdit(int X, int Y, int W, int H) : Fl_Input(X, Y, W, H) {}
    int handle(int event) override
    {
        if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape) {
            hide();
            if (parent()) parent()->redraw();
            return 1;
        }
        if (event == FL_UNFOCUS) {
            const int r = Fl_Input::handle(event);
            hide();
            if (parent()) parent()->redraw();
            return r;
        }
        return Fl_Input::handle(event);
    }
};

class RegistersView : public Panel {
public:
    RegistersView(int X, int Y, int W, int H) : Panel(X, Y, W, H, "CPU")
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
    }

    static int preferred_height()
    {
        const int lh = data_line_height();
        /* registers, flag chips, interrupt chips, (HL)/(SP), timing */
        return HEADER_H + 8 + 7 * lh + 22 + (lh + 4) + 18 + (lh + 4) + 12 + 2 * lh + TIMING_GAP + 2 * lh + 8;
    }

    void refresh(bool paused, bool force) override
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

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        draw_header(x(), y(), w(), "CPU", extra_text());
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

    int handle(int event) override
    {
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

private:
    void cell(int id, int& name_x, int& value_x, int& row_y) const
    {
        const int lh = data_line_height();
        const int cw = char_width();
        const int col = id / 7;
        const int row = id % 7;
        name_x = x() + 10 + col * (w() / 2);
        value_x = name_x + cw * 4;
        row_y = y() + HEADER_H + 8 + row * lh;
    }

    int flags_y() const
    {
        return y() + HEADER_H + 8 + 7 * data_line_height() + 22;
    }

    void chip(int i, int& cx, int& cw) const
    {
        cw = (w() - 20) / 8 - 4;
        if (cw > 30) cw = 30;
        cx = x() + 10 + i * (cw + 4);
    }

    static constexpr int INT_CHIPS = 6;   /* IM n, IFF1, IFF2, HALT, INT, NMI */
    static constexpr int TIMING_GAP = 9;  /* space holding the divider above the timing lines */

    int int_y() const { return flags_y() + data_line_height() + 4 + 18; }
    int timing_y() const { return int_y() + 3 * data_line_height() + 4 + 12 + TIMING_GAP; }

    void int_chip(int i, int& cx, int& cw) const
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

    int hit_int_chip(int ex, int ey) const
    {
        const int iy = int_y();
        if (ey < iy || ey >= iy + data_line_height() + 4) return -1;
        for (int i = 0; i < INT_CHIPS; i++) {
            int cx, cw;
            int_chip(i, cx, cw);
            if (ex >= cx && ex < cx + cw) return i;
        }
        return -1;
    }

    bool hit_timing(int ey) const
    {
        const int ty = timing_y();
        return ey >= ty && ey < ty + 2 * data_line_height();
    }

    static void group_digits(char* out, size_t size, uint64_t value)
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

    /* CYCLES / FRAME counters and the ZVB raster position (decimal) */
    void draw_timing(int lh, int cw, int asc)
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

    void tooltip_extra(int hover)
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

    static constexpr int HOVER_INT = 100;
    static constexpr int HOVER_TIMING = 200;

    int hit_value(int ex, int ey) const
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

    int hit_name(int ex, int ey) const
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

    int hit_chip(int ex, int ey) const
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

    void tooltip_for(int id)
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

    void context_menu(int id)
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

    void begin_edit(int id)
    {
        int nx, vx, ry;
        cell(id, nx, vx, ry);
        char text[8];
        if (reg_is_8bit(id)) hex8(text, sizeof(text), reg_get(m_regs, id));
        else hex16(text, sizeof(text), reg_get(m_regs, id));
        m_edit_id = id;
        m_edit->resize(vx - 3, ry, char_width() * 4 + 8, data_line_height());
        m_edit->maximum_size(reg_is_8bit(id) ? 2 : 4);
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

    static void cb_commit(Fl_Widget*, void* data)
    {
        RegistersView* self = static_cast<RegistersView*>(data);
        const char* text = self->m_edit->value();
        char* end = nullptr;
        const unsigned long v = strtoul(text[0] == '$' ? text + 1 : text, &end, 16);
        self->m_edit->hide();
        if (end == text || (end && *end) || !dbg_ready() || !fz_paused()) {
            fl_beep();
        } else {
            regs_t regs = exact_regs();
            reg_set(regs, self->m_edit_id, (uint16_t) v);
            debugger_set_registers(fz_debugger(), &regs);
        }
        request_refresh();
    }

    InlineEdit* m_edit;
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

/* ------------------------------------------------------------------ */
/*  Stack                                                              */
/* ------------------------------------------------------------------ */

class StackView : public Panel {
public:
    StackView(int X, int Y, int W, int H) : Panel(X, Y, W, H, "STACK")
    {
        resizable(nullptr);
        end();
        tooltip("Click: show the slot in memory    Right-click: more");
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        m_regs = current_regs();
        char text[32];
        char sp[8];
        hex16(sp, sizeof(sp), m_regs.sp);
        if (m_first > 0) snprintf(text, sizeof(text), "SP $%s  +%X", sp, m_first * 2);
        else snprintf(text, sizeof(text), "SP $%s", sp);
        extra(text);
        redraw();
    }

    void draw() override
    {
        Panel::draw();
        if (!dbg_ready()) return;
        const int lh = data_line_height();
        const int cw = char_width();
        const int asc = text_baseline(lh);
        const int rows = (h() - HEADER_H - 8) / lh;
        /* Only whole words between SP and the end of the address space can be
         * stack (it grows down; never wrap to $0000); while running, only the
         * sampled 32 words exist. Never scroll past a full view. */
        const int limit = stack_limit();
        clamp_first(rows, limit);
        int count = rows < 128 ? rows : 128;
        if (m_first + count > limit) count = limit - m_first;
        m_count = count > 0 ? count : 0;
        if (limit <= 0) {
            fl_font(FL_HELVETICA, 11);
            fl_color(COLOR_DIM);
            fl_draw("Empty: SP is at the top of memory.", x() + 10, top() + 4, w() - 20, lh,
                    FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_CLIP);
            return;
        }
        if (count <= 0) return;
        uint8_t mem[256 + 64];
        if (fz_paused()) {
            debugger_read_memory(fz_debugger(), (m_regs.sp + m_first * 2) & 0xFFFF, count * 2, mem);
        } else {
            uint8_t sample[64];
            display_memory(m_regs.sp, 64, sample, true);
            memcpy(mem, sample + m_first * 2, (size_t) count * 2);
        }
        fl_push_clip(x(), top(), w(), h() - HEADER_H);
        fl_font(DATA_FONT, DATA_SIZE);
        char text[16];
        for (int i = 0; i < count; i++) {
            const int ry = top() + 4 + i * lh;
            const int index = m_first + i;
            const unsigned addr = (m_regs.sp + index * 2) & 0xFFFF;
            const unsigned word = mem[i * 2] | (mem[i * 2 + 1] << 8);
            if (i < (int) (sizeof(m_words) / sizeof(m_words[0]))) m_words[i] = (uint16_t) word;
            if (i == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x(), ry, w(), lh);
            }
            if (index == 0) {
                fl_color(COLOR_AMBER);
                fl_draw(">", x() + 6, ry + asc);
            }
            fl_color(COLOR_DIM);
            snprintf(text, sizeof(text), index * 2 < 0x100 ? "+%02X" : "+%X", index * 2);
            fl_draw(text, x() + 6 + cw * 2, ry + asc);
            hex16(text, sizeof(text), addr);
            fl_color(COLOR_CYAN);
            fl_draw(text, x() + 6 + cw * 6, ry + asc);
            hex16(text, sizeof(text), word);
            fl_color(COLOR_TEXT);
            fl_draw(text, x() + 6 + cw * 12, ry + asc);
            const char* sym = debugger_get_symbol(fz_debugger(), word);
            if (sym) {
                fl_color(COLOR_AMBER);
                fl_draw(sym, x() + 6 + cw * 17, ry + asc);
            }
        }
        fl_pop_clip();
    }

    int handle(int event) override
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
                clamp_first((h() - HEADER_H - 8) / data_line_height(), stack_limit());
                {
                    char text[32], sp[8];
                    hex16(sp, sizeof(sp), m_regs.sp);
                    if (m_first > 0) snprintf(text, sizeof(text), "SP $%s  +%X", sp, m_first * 2);
                    else snprintf(text, sizeof(text), "SP $%s", sp);
                    extra(text);
                }
                redraw();
                return 1;
            case FL_PUSH: {
                const int r = row_at(Fl::event_y());
                if (r < 0 || !dbg_ready()) return 1;
                const hwaddr addr = (m_regs.sp + (m_first + r) * 2) & 0xFFFF;
                if (Fl::event_button() == FL_RIGHT_MOUSE) {
                    context_menu(addr, m_words[r]);
                } else {
                    goto_memory(addr);
                }
                return 1;
            }
            default:
                break;
        }
        return Fl_Group::handle(event);
    }

private:
    /** Words from SP up to the top of memory (or the sample while running). */
    int stack_limit() const
    {
        const int words = (0x10000 - (int) m_regs.sp) / 2;
        const int sampled = fz_paused() ? words : 32;
        return words < sampled ? words : sampled;
    }

    void clamp_first(int rows, int limit)
    {
        int max_first = limit - (rows < limit ? rows : limit);
        if (max_first < 0) max_first = 0;
        if (m_first > max_first) m_first = max_first;
        if (m_first < 0) m_first = 0;
    }

    int row_at(int ey) const
    {
        if (ey < top() + 4) return -1;
        const int r = (ey - top() - 4) / data_line_height();
        return r < m_count ? r : -1;
    }

    void context_menu(hwaddr slot, uint16_t value)
    {
        Fl_Menu_Item items[] = {
            { "Show Slot in Memory", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Show Value in Memory", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Show Value in Disassembly", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Copy Value", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Copy Address", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        };
        const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
        if (picked == nullptr) return;
        char text[8];
        switch ((int) (picked - items)) {
            case 0: goto_memory(slot); break;
            case 1: goto_memory(value); break;
            case 2: goto_disassembly(value); break;
            case 3:
                hex16(text, sizeof(text), value);
                Fl::copy(text, (int) strlen(text), 1);
                break;
            case 4:
                hex16(text, sizeof(text), slot);
                Fl::copy(text, (int) strlen(text), 1);
                break;
            default: break;
        }
    }

    regs_t m_regs = {};
    int m_hover = -1;
    int m_first = 0; /* first word shown, relative to SP (mouse wheel) */
    uint16_t m_words[128] = {};
    int m_count = 0; /* rows drawn */
};

/* ------------------------------------------------------------------ */
/*  Breakpoints                                                        */
/* ------------------------------------------------------------------ */

/** Filled red dot = enabled breakpoint, red ring = disabled. */
static void draw_breakpoint_marker(int X, int Y, int size, bool enabled)
{
    fl_color(COLOR_BREAK);
    if (enabled) {
        fl_pie(X, Y, size, size, 0, 360);
    } else {
        fl_line_style(FL_SOLID, 2);
        fl_arc(X + 1, Y + 1, size - 2, size - 2, 0, 360);
        fl_line_style(0);
    }
}

class BreakpointList : public Fl_Widget {
public:
    BreakpointList(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H) {}

    std::vector<hwaddr> addrs;

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        const int lh = data_line_height();
        const int cw = char_width();
        const int asc = text_baseline(lh);
        fl_push_clip(x(), y(), w(), h());
        if (addrs.empty()) {
            fl_color(COLOR_DIM);
            fl_font(FL_HELVETICA, 11);
            fl_draw("No breakpoints. Click the disassembly gutter or add one below.",
                    x() + 8, y() + 4, w() - 16, lh * 2, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
        }
        fl_font(DATA_FONT, DATA_SIZE);
        char text[8];
        for (size_t i = 0; i < addrs.size(); i++) {
            const int ry = y() + 2 + (int) i * lh;
            const bool enabled = debugger_is_breakpoint_enabled(fz_debugger(), addrs[i]);
            if ((int) i == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x(), ry, w(), lh);
                fl_color(COLOR_BREAK);
                fl_draw("x", x() + w() - cw * 2, ry + asc);
            }
            draw_breakpoint_marker(x() + 7, ry + (lh - 11) / 2, 11, enabled);
            hex16(text, sizeof(text), addrs[i]);
            fl_color(enabled ? COLOR_CYAN : COLOR_DIM);
            fl_draw(text, x() + 26, ry + asc);
            const char* sym = debugger_get_symbol(fz_debugger(), addrs[i]);
            if (sym) {
                fl_color(enabled ? COLOR_AMBER : COLOR_DIM);
                fl_draw(sym, x() + 26 + cw * 6, ry + asc);
            }
            if (!enabled) {
                fl_font(FL_HELVETICA, 10);
                fl_color(COLOR_DIM);
                fl_draw("off", x() + w() - cw * 2 - 30, ry + asc);
                fl_font(DATA_FONT, DATA_SIZE);
            }
        }
        fl_pop_clip();
    }

    int handle(int event) override
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
                return 1;
            case FL_PUSH: {
                const int r = row_at(Fl::event_y());
                if (Fl::event_button() == FL_RIGHT_MOUSE) {
                    context_menu(r);
                    return 1;
                }
                if (r < 0) return 1;
                dbg_t* dbg = fz_debugger();
                if (Fl::event_x() < x() + 24) {
                    debugger_enable_breakpoint(dbg, addrs[r], !debugger_is_breakpoint_enabled(dbg, addrs[r]));
                    request_refresh();
                } else if (Fl::event_x() >= x() + w() - char_width() * 3) {
                    debugger_clear_breakpoint(dbg, addrs[r]);
                    request_refresh();
                } else {
                    goto_disassembly(addrs[r]);
                }
                return 1;
            }
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

private:
    int row_at(int ey) const
    {
        const int r = (ey - y() - 2) / data_line_height();
        return ey >= y() + 2 && r < (int) addrs.size() ? r : -1;
    }

    void context_menu(int row)
    {
        dbg_t* dbg = fz_debugger();
        const bool has_row = row >= 0;
        const bool enabled = has_row && debugger_is_breakpoint_enabled(dbg, addrs[row]);
        const int row_only = has_row ? 0 : FL_MENU_INACTIVE;
        const int any = addrs.empty() ? FL_MENU_INACTIVE : 0;
        Fl_Menu_Item items[] = {
            { enabled ? "Disable" : "Enable", 0, nullptr, nullptr, row_only, 0, 0, 0, 0 },
            { "Remove", 0, nullptr, nullptr, row_only, 0, 0, 0, 0 },
            { "Show in Disassembly", 0, nullptr, nullptr, row_only | FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Enable All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
            { "Disable All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
            { "Remove All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
            { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        };
        const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
        if (picked == nullptr) return;
        const std::vector<hwaddr> all = addrs;
        switch ((int) (picked - items)) {
            case 0: debugger_enable_breakpoint(dbg, addrs[row], !enabled); break;
            case 1: debugger_clear_breakpoint(dbg, addrs[row]); break;
            case 2: goto_disassembly(addrs[row]); return;
            case 3: for (hwaddr a : all) debugger_enable_breakpoint(dbg, a, true); break;
            case 4: for (hwaddr a : all) debugger_enable_breakpoint(dbg, a, false); break;
            case 5: for (hwaddr a : all) debugger_clear_breakpoint(dbg, a); break;
            default: break;
        }
        request_refresh();
    }

    int m_hover = -1;
};

class BreakpointsPanel : public Panel {
public:
    BreakpointsPanel(int X, int Y, int W, int H) : Panel(X, Y, W, H, "BREAKPOINTS")
    {
        m_list = new BreakpointList(X, Y, 10, 10);
        m_input = new Fl_Input(X, Y, 10, 10);
        style_input(m_input);
        m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
        m_input->callback(cb_add, this);
        m_input->tooltip("Address (hex) or symbol name, then Enter\nPart of a name lists the matching symbols");
        m_add = new Fl_Button(X, Y, 10, 10, "Add");
        style_button(m_add);
        m_add->callback(cb_add, this);
        m_list->tooltip("Dot: enable/disable    Address: show in disassembly    x: remove    Right-click: more");
        resizable(m_list);
        end();
        resize(X, Y, W, H);
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        if (!dbg_ready()) return;
        hwaddr bps[DBG_MAX_POINTS];
        const int count = debugger_get_breakpoints(fz_debugger(), bps, DBG_MAX_POINTS);
        m_list->addrs.assign(bps, bps + (count > 0 ? count : 0));
        int off = 0;
        for (hwaddr a : m_list->addrs) off += !debugger_is_breakpoint_enabled(fz_debugger(), a);
        char text[32];
        if (off > 0) snprintf(text, sizeof(text), "%d  (%d OFF)", count, off);
        else snprintf(text, sizeof(text), "%d", count > 0 ? count : 0);
        extra(text);
        m_list->redraw();
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        const int iy = Y + H - BAR_H + 4;
        m_list->resize(X, Y + HEADER_H, W, H - HEADER_H - BAR_H);
        m_input->resize(X + 6, iy, W - 72, BAR_H - 8);
        m_add->resize(X + W - 62, iy, 56, BAR_H - 8);
    }

private:
    static void cb_add(Fl_Widget*, void* data)
    {
        BreakpointsPanel* self = static_cast<BreakpointsPanel*>(data);
        hwaddr addr;
        std::string name;
        if (dbg_ready() && lookup_address(self->m_input, self->m_input->value(), &addr, SYM_KIND_CODE, &name)) {
            debugger_set_breakpoint(fz_debugger(), addr);
            self->m_input->value("");
            char text[16];
            hex16(text, sizeof(text), addr);
            if (name.empty()) notif_show("Breakpoint at $%s", text);
            else notif_show("Breakpoint at %s ($%s)", name.c_str(), text);
            request_refresh();
        } else {
            fl_beep();
        }
    }

    BreakpointList* m_list;
    Fl_Input* m_input;
    Fl_Button* m_add;
};

/* ------------------------------------------------------------------ */
/*  Disassembly                                                        */
/* ------------------------------------------------------------------ */

class DisasmView : public Fl_Widget {
public:
    DisasmView(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H) {}

    int selected = -1;

    void go(hwaddr addr)
    {
        m_base = addr & 0xFFFF;
        m_history.clear();
        redraw();
    }

    /** Keep PC visible without jumping on every step. */
    void follow(hwaddr pc)
    {
        for (size_t i = 0; i + 3 < m_rows.size(); i++) {
            if (m_rows[i].addr == pc) return;
        }
        go(pc);
    }

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        fl_color(COLOR_BG);
        fl_rectf(x(), y(), GUTTER, h());
        if (!dbg_ready()) {
            fl_pop_clip();
            return;
        }
        dbg_t* dbg = fz_debugger();
        const regs_t regs = current_regs();
        const bool paused = fz_paused();
        const int lh = data_line_height();
        const int cw = char_width();
        const int asc = text_baseline(lh);
        const int x_addr = x() + GUTTER + 6;
        const int x_bytes = x_addr + cw * 6;
        const int x_instr = x_bytes + cw * 12;
        fl_font(DATA_FONT, DATA_SIZE);

        m_rows.clear();
        hwaddr addr = m_base;
        int yy = y() + 4;
        char buf[96];
        while (yy + lh <= y() + h() && addr <= 0xFFFF) {
            dbg_instr_t instr;
            memset(&instr, 0, sizeof(instr));
            const int size = debugger_disassemble_address(dbg, addr, &instr);
            if (instr.label[0]) {
                if (yy > y() + 4) yy += 4;
                fl_color(COLOR_AMBER);
                snprintf(buf, sizeof(buf), "%s:", instr.label);
                fl_draw(buf, x_addr, yy + asc);
                yy += lh;
                if (yy + lh > y() + h()) break;
            }

            const bool is_pc = paused && addr == regs.pc;
            const bool is_bp = debugger_has_breakpoint(dbg, addr);
            const bool bp_on = is_bp && debugger_is_breakpoint_enabled(dbg, addr);
            if (is_pc) {
                fl_color(COLOR_PC);
                fl_rectf(x() + GUTTER, yy, w() - GUTTER, lh);
                fl_color(COLOR_GREEN);
                fl_rectf(x() + GUTTER, yy, 2, lh);
            } else if ((int) addr == selected) {
                fl_color(COLOR_SELECT);
                fl_rectf(x() + GUTTER, yy, w() - GUTTER, lh);
            } else if ((int) addr == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x() + GUTTER, yy, w() - GUTTER, lh);
            }
            if (is_bp) {
                draw_breakpoint_marker(x() + 5, yy + (lh - 10) / 2, 10, bp_on);
            } else if ((int) addr == m_hover) {
                fl_color(COLOR_BORDER);
                fl_pie(x() + 5, yy + (lh - 10) / 2, 10, 10, 0, 360);
            }
            if (is_pc) {
                fl_color(COLOR_GREEN);
                const int cy = yy + lh / 2;
                fl_polygon(x() + 11, cy - 5, x() + 19, cy, x() + 11, cy + 5);
            }

            hex16(buf, sizeof(buf), addr);
            fl_color(is_pc ? COLOR_GREEN : COLOR_CYAN);
            fl_draw(buf, x_addr, yy + asc);

            const int n = size > 0 ? (size > 4 ? 4 : size) : 1;
            int bx = 0;
            for (int i = 0; i < n; i++) {
                bx += snprintf(buf + bx, sizeof(buf) - bx, hex8_fmt(), instr.opcodes[i]);
                buf[bx++] = ' ';
                buf[bx] = '\0';
            }
            fl_color(COLOR_DIM);
            fl_draw(buf, x_bytes, yy + asc);

            fl_color(is_pc ? COLOR_GREEN : COLOR_TEXT);
            fl_draw(instr.instruction, x_instr, yy + asc);

            m_rows.push_back({ yy, lh, addr });
            yy += lh;
            if (size <= 0) break;
            addr += size;
        }
        fl_pop_clip();
    }

    int handle(int event) override
    {
        switch (event) {
            case FL_ENTER:
                return 1;
            case FL_LEAVE:
                m_hover = -1;
                redraw();
                return 1;
            case FL_MOVE: {
                const int hover = row_at(Fl::event_y());
                if (hover != m_hover) {
                    m_hover = hover;
                    redraw();
                }
                return 1;
            }
            case FL_PUSH: {
                const int addr = row_at(Fl::event_y());
                if (addr < 0) return 1;
                if (Fl::event_button() == FL_RIGHT_MOUSE) {
                    selected = addr;
                    redraw();
                    context_menu((hwaddr) addr);
                    return 1;
                }
                if (Fl::event_x() < x() + GUTTER || Fl::event_clicks() > 0) {
                    debugger_toggle_breakpoint(fz_debugger(), (hwaddr) addr);
                    Fl::event_clicks(0);
                    request_refresh();
                } else {
                    selected = addr;
                    redraw();
                }
                return 1;
            }
            case FL_MOUSEWHEEL: {
                const int dy = Fl::event_dy();
                const int steps = (dy > 0 ? dy : -dy) * 3;
                for (int i = 0; i < steps; i++) {
                    if (dy > 0) {
                        dbg_instr_t instr;
                        const int size = debugger_disassemble_address(fz_debugger(), m_base, &instr);
                        const hwaddr next = (hwaddr) (m_base + (size > 0 ? size : 1));
                        if (next > 0xFFFF) break;
                        m_history.push_back(m_base);
                        m_base = next;
                    } else if (!m_history.empty()) {
                        m_base = m_history.back();
                        m_history.pop_back();
                    } else if (m_base > 0) {
                        m_base--;
                    }
                }
                redraw();
                return 1;
            }
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

private:
    static constexpr int GUTTER = 22;
    struct Row { int y; int h; hwaddr addr; };

    int row_at(int ey) const
    {
        for (const Row& r : m_rows) {
            if (ey >= r.y && ey < r.y + r.h) return (int) r.addr;
        }
        return -1;
    }

    void context_menu(hwaddr addr)
    {
        const int pause_only = fz_paused() ? 0 : FL_MENU_INACTIVE;
        dbg_t* bp_dbg = fz_debugger();
        const bool has_bp = debugger_has_breakpoint(bp_dbg, addr);
        const bool bp_on = has_bp && debugger_is_breakpoint_enabled(bp_dbg, addr);
        Fl_Menu_Item items[] = {
            { "Run to Cursor", 0, nullptr, nullptr, pause_only, 0, 0, 0, 0 },
            { "Set PC Here", 0, nullptr, nullptr, pause_only | FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { has_bp ? "Remove Breakpoint" : "Add Breakpoint", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { bp_on ? "Disable Breakpoint" : "Enable Breakpoint", 0, nullptr, nullptr,
              (has_bp ? 0 : FL_MENU_INACTIVE) | FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Show in Memory", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Copy Address", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        };
        const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
        if (picked == nullptr) return;
        dbg_t* dbg = fz_debugger();
        char text[8];
        switch ((int) (picked - items)) {
            case 0:
                debugger_set_temporary_breakpoint(dbg, addr);
                debugger_continue(dbg);
                break;
            case 1: {
                regs_t regs = exact_regs();
                regs.pc = (uint16_t) addr;
                debugger_set_registers(dbg, &regs);
                break;
            }
            case 2:
                debugger_toggle_breakpoint(dbg, addr);
                break;
            case 3:
                debugger_enable_breakpoint(dbg, addr, !bp_on);
                break;
            case 4:
                goto_memory(addr);
                break;
            case 5:
                hex16(text, sizeof(text), addr);
                Fl::copy(text, (int) strlen(text), 1);
                break;
            default:
                break;
        }
        request_refresh();
    }

    hwaddr m_base = 0;
    std::vector<hwaddr> m_history;
    std::vector<Row> m_rows;
    int m_hover = -1;
};

class DisasmPanel : public Panel {
public:
    DisasmPanel(int X, int Y, int W, int H) : Panel(X, Y, W, H, "DISASSEMBLY")
    {
        m_input = new Fl_Input(X, Y, 10, 10);
        style_input(m_input);
        m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
        m_input->callback(cb_go, this);
        m_input->tooltip("Address (hex) or symbol name, then Enter\nPart of a name lists the matching symbols");
        m_pc = new Fl_Button(X, Y, 10, 10, "PC");
        style_button(m_pc);
        m_pc->callback(cb_pc, this);
        m_pc->tooltip("Jump to the current PC and follow it");
        m_follow = new Fl_Check_Button(X, Y, 10, 10, "Follow PC");
        style_check(m_follow);
        m_follow->value(1);
        m_view = new DisasmView(X, Y, 10, 10);
        m_view->tooltip("Gutter click / double-click: breakpoint    Right-click: more");
        resizable(m_view);
        end();
        resize(X, Y, W, H);
    }

    void refresh(bool paused, bool force) override
    {
        if (paused && force && m_follow->value()) m_view->follow(current_regs().pc);
        m_view->redraw();
    }

    void go(hwaddr addr)
    {
        m_follow->value(0);
        m_view->selected = (int) addr;
        m_view->go(addr);
    }

    int selected() const { return m_view->selected; }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        const int by = Y + HEADER_H + 4;
        m_input->resize(X + 6, by, 140, BAR_H - 8);
        m_pc->resize(X + 150, by, 40, BAR_H - 8);
        m_follow->resize(X + 196, by, 96, BAR_H - 8);
        m_view->resize(X, Y + HEADER_H + BAR_H, W, H - HEADER_H - BAR_H);
    }

private:
    static void cb_go(Fl_Widget*, void* data)
    {
        DisasmPanel* self = static_cast<DisasmPanel*>(data);
        hwaddr addr;
        if (lookup_address(self->m_input, self->m_input->value(), &addr, SYM_KIND_CODE)) self->go(addr);
        else fl_beep();
    }

    static void cb_pc(Fl_Widget*, void* data)
    {
        DisasmPanel* self = static_cast<DisasmPanel*>(data);
        self->m_follow->value(1);
        self->m_view->go(current_regs().pc);
    }

    Fl_Input* m_input;
    Fl_Button* m_pc;
    Fl_Check_Button* m_follow;
    DisasmView* m_view;
};

/* ------------------------------------------------------------------ */
/*  Memory editor with MMU page strip                                  */
/* ------------------------------------------------------------------ */

#include "cp437.inc"

/** The four 16 KB pages of the Z80 address space and what is mapped there. */
class MmuStrip : public Fl_Widget {
public:
    MmuStrip(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H)
    {
        tooltip("MMU: physical address and device of each 16 KB page. Click a page to view it.");
    }

    void refresh()
    {
        m_valid = dbg_ready() && debugger_custom(fz_debugger(), ZEAL_DBG_OP_GET_MMU, &m_info);
        redraw();
    }

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        if (!m_valid) return;
        const int seg_w = (w() - 8) / ZEAL_DBG_MMU_PAGES;
        char text[64], v[8];
        for (int i = 0; i < ZEAL_DBG_MMU_PAGES; i++) {
            const dbg_mmu_entry_t* e = &m_info.entries[i];
            const int sx = x() + 4 + i * seg_w;
            fl_color(COLOR_RAISED);
            fl_rectf(sx, y() + 3, seg_w - 3, h() - 6);
            fl_color(device_color(e->device));
            fl_rectf(sx, y() + 3, 2, h() - 6);
            hex16(v, sizeof(v), e->virt_addr);
            snprintf(text, sizeof(text), config.debugger.hex_upper ? "%s  %05X  %s" : "%s  %05x  %s",
                     v, (unsigned) e->phys_addr, e->device ? e->device : "--");
            fl_font(FL_HELVETICA, 11);
            fl_color(e->device ? COLOR_TEXT : COLOR_DIM);
            fl_draw(text, sx + 7, y() + 3, seg_w - 12, h() - 6, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
        }
    }

    int handle(int event) override
    {
        if (event == FL_MOUSEWHEEL) return 1;
        if (event == FL_PUSH && m_valid) {
            const int seg_w = (w() - 8) / ZEAL_DBG_MMU_PAGES;
            const int i = (Fl::event_x() - x() - 4) / seg_w;
            if (i >= 0 && i < ZEAL_DBG_MMU_PAGES) goto_memory(m_info.entries[i].virt_addr);
            return 1;
        }
        return Fl_Widget::handle(event);
    }

private:
    static Fl_Color device_color(const char* name)
    {
        if (name == nullptr) return COLOR_BORDER;
        unsigned hsh = 0;
        for (const char* p = name; *p; p++) hsh = hsh * 31 + (unsigned char) *p;
        static const Fl_Color palette[] = { COLOR_AMBER, COLOR_CYAN, COLOR_GREEN, COLOR_CHANGED };
        return palette[hsh % 4];
    }

    dbg_mmu_t m_info = {};
    bool m_valid = false;
};

class MemView : public Fl_Widget {
public:
    static constexpr int COLS = 16;
    static constexpr int ROWS_TOTAL = 0x10000 / COLS;
    static constexpr Fl_Color COLOR_MATCH = 0x3D3320'00; /* search hits */

    MemView(int X, int Y, int W, int H, Fl_Scrollbar* bar)
        : Fl_Widget(X, Y, W, H), m_bar(bar)
    {
        m_prev.resize(0x10000);
        m_snapshot.resize(0x10000);
    }

    bool cp437 = false;
    int follow_addr = -1;
    /* Hooks set by the panel */
    std::function<void()> on_selection;
    std::function<void()> on_find;
    std::function<void()> on_changes;
    std::function<void(hwaddr)> on_goto;

    /** Selection: from the anchor to the cursor byte, inclusive. */
    int sel_lo() const { return m_anchor < m_sel ? m_anchor : m_sel; }
    int sel_hi() const { return m_anchor > m_sel ? m_anchor : m_sel; }
    int sel_len() const { return m_sel < 0 ? 0 : sel_hi() - sel_lo() + 1; }
    int top_addr() const { return m_top * COLS; }

    /** Show `addr`; `select` moves the cursor there, `extend` keeps the anchor. */
    void go(hwaddr addr, bool select = true, bool extend = false)
    {
        addr &= 0xFFFF;
        if (select) {
            m_sel = (int) addr;
            if (!extend || m_anchor < 0) m_anchor = m_sel;
            m_nibble = -1;
            selection_changed();
        }
        const int row = (int) addr / COLS;
        const int rows = visible_rows();
        if (row < m_top || row >= m_top + rows - 1) m_top = row - (select ? 0 : rows / 3);
        clamp_top();
        sync_bar();
        redraw();
    }

    /** Select [lo, hi] and bring it into view (a third down when scrolling). */
    void select_range(int lo, int hi)
    {
        m_anchor = lo & 0xFFFF;
        m_sel = hi & 0xFFFF;
        m_nibble = -1;
        const int rows = visible_rows();
        if (lo / COLS < m_top || hi / COLS >= m_top + rows - 1) m_top = lo / COLS - rows / 3;
        clamp_top();
        sync_bar();
        selection_changed();
        redraw();
    }

    /** Tint every visible occurrence of `p` (empty pattern: none). */
    void highlight(const memtools::Pattern& p)
    {
        m_find = p;
        redraw();
    }

    void snapshot()
    {
        if (!dbg_ready()) return;
        m_prev.swap(m_snapshot);
        debugger_read_memory(fz_debugger(), 0, 0x10000, m_snapshot.data());
        m_have_prev = m_have_snapshot;
        m_have_snapshot = true;
    }

    bool snapshots(const uint8_t** prev, const uint8_t** cur) const
    {
        if (!m_have_snapshot) return false;
        *prev = m_have_prev ? m_prev.data() : nullptr;
        *cur = m_snapshot.data();
        return true;
    }

    /** Write through the debugger; edits do not count as "changed". */
    void write(unsigned addr, const std::vector<uint8_t>& data)
    {
        if (!dbg_ready() || data.empty() || addr + data.size() > 0x10000) return;
        std::vector<uint8_t> copy(data);
        debugger_write_memory(fz_debugger(), (hwaddr) addr, (int) copy.size(), copy.data());
        memcpy(&m_snapshot[addr], copy.data(), copy.size());
        request_refresh();
    }

    void scroll_to_row(int row)
    {
        m_top = row;
        clamp_top();
        redraw();
    }

    int visible_rows() const { return (h() - 4) / data_line_height(); }

    void sync_bar() { m_bar->value(m_top, visible_rows(), 0, ROWS_TOTAL); }

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        if (!dbg_ready()) {
            fl_pop_clip();
            return;
        }
        const int lh = data_line_height();
        const int asc = text_baseline(lh);
        geometry(char_width());
        const int rows = visible_rows();

        uint8_t mem[COLS * 128];
        const int count_rows = rows < 128 ? rows : 128;
        const int start = m_top * COLS;
        int len = count_rows * COLS;
        if (start + len > 0x10000) len = 0x10000 - start;
        if (len > 0) debugger_read_memory(fz_debugger(), (hwaddr) start, len, mem);
        bool match[COLS * 128] = {};
        if (len > 0) mark_matches(start, len, match);

        /* Faint guides every 4 bytes */
        fl_color(COLOR_BG);
        for (int g = 4; g < COLS; g += 4) fl_rectf(m_hex_x + hex_offset(g) - m_cw + m_cw / 2, y(), 1, h());

        fl_font(DATA_FONT, DATA_SIZE);
        char buf[16];
        const bool paused = fz_paused();
        const int lo = sel_lo();
        const int hi = sel_hi();
        const bool range = m_sel >= 0 && hi > lo;
        for (int r = 0; r < count_rows; r++) {
            const int row_addr = start + r * COLS;
            if (row_addr >= 0x10000) break;
            const int yy = y() + 2 + r * lh;
            hex16(buf, sizeof(buf), (unsigned) row_addr);
            fl_color(COLOR_CYAN);
            fl_draw(buf, x() + 6, yy + asc);
            for (int c = 0; c < COLS; c++) {
                const int addr = row_addr + c;
                const uint8_t b = mem[r * COLS + c];
                const bool sel = m_sel >= 0 && addr >= lo && addr <= hi;
                const bool cursor = addr == m_sel;
                const bool hover = addr == m_hover;
                const bool hit = match[r * COLS + c];
                const bool followed = follow_addr >= 0 && (addr == follow_addr || addr == follow_addr + 1);
                const bool changed = m_have_prev && paused && b != m_prev[addr];
                if (sel || hover || followed || hit) {
                    fl_color(sel ? COLOR_SELECT : followed ? COLOR_PC : hit ? COLOR_MATCH : COLOR_HOVER);
                    /* Contiguous selection/match bands: also fill the gap to the next byte */
                    const bool band = (sel && addr < hi && c < COLS - 1) || (!sel && hit && c < COLS - 1 &&
                                                                              match[r * COLS + c + 1]);
                    const int gap = band ? hex_offset(c + 1) - hex_offset(c) - m_cw * 2 : 4;
                    fl_rectf(m_hex_x + hex_offset(c) - 2, yy, m_cw * 2 + (band ? gap : 4), lh);
                    fl_rectf(m_ascii_x + c * m_cw, yy, m_cw, lh);
                }
                if (cursor && range) {
                    fl_color(COLOR_CYAN);
                    fl_rect(m_hex_x + hex_offset(c) - 2, yy, m_cw * 2 + 4, lh);
                }
                if (cursor && m_nibble >= 0) {
                    snprintf(buf, sizeof(buf), config.debugger.hex_upper ? "%X_" : "%x_", m_nibble);
                    fl_color(COLOR_AMBER);
                } else {
                    hex8(buf, sizeof(buf), b);
                    fl_color(changed ? COLOR_CHANGED : hit ? COLOR_AMBER : b == 0 ? COLOR_DIM : COLOR_TEXT);
                }
                fl_draw(buf, m_hex_x + hex_offset(c), yy + asc);

                char ch[8];
                glyph(b, ch);
                fl_color(changed ? COLOR_CHANGED : hit ? COLOR_AMBER : (isprint(b) || cp437) ? COLOR_TEXT : COLOR_DIM);
                fl_draw(ch, m_ascii_x + c * m_cw, yy + asc);
            }
        }
        fl_pop_clip();
    }

    int handle(int event) override
    {
        switch (event) {
            case FL_ENTER:
                return 1;
            case FL_LEAVE:
                m_hover = -1;
                redraw();
                return 1;
            case FL_MOVE: {
                const int a = addr_at(Fl::event_x(), Fl::event_y());
                if (a != m_hover) {
                    m_hover = a;
                    redraw();
                    if (a >= 0 && dbg_ready()) {
                        static char tip[64];
                        uint8_t b = 0;
                        debugger_read_memory(fz_debugger(), (hwaddr) a, 1, &b);
                        char ad[8];
                        hex16(ad, sizeof(ad), (unsigned) a);
                        const char* sym = debugger_get_symbol(fz_debugger(), (hwaddr) a);
                        snprintf(tip, sizeof(tip), "$%s = %u%s%s", ad, b, sym ? "   " : "", sym ? sym : "");
                        Fl_Tooltip::enter_area(this, Fl::event_x() - x() - 4, Fl::event_y() - y() - 4, 8, 8, tip);
                    }
                }
                return 1;
            }
            case FL_PUSH: {
                take_focus();
                const int a = addr_at(Fl::event_x(), Fl::event_y());
                m_dragging = false;
                if (a < 0) return 1;
                if (Fl::event_button() == FL_RIGHT_MOUSE) {
                    if (m_sel < 0 || a < sel_lo() || a > sel_hi()) {
                        m_sel = m_anchor = a;
                        m_nibble = -1;
                        selection_changed();
                    }
                    redraw();
                    context_menu();
                    return 1;
                }
                m_sel = a;
                if (!(Fl::event_state() & FL_SHIFT) || m_anchor < 0) m_anchor = a;
                m_nibble = -1;
                m_dragging = Fl::event_button() == FL_LEFT_MOUSE;
                selection_changed();
                redraw();
                return 1;
            }
            case FL_DRAG: {
                if (!m_dragging) return 1;
                /* Past the top/bottom edge: scroll one row and extend */
                if (Fl::event_y() < y() || Fl::event_y() >= y() + h()) {
                    scroll_to_row(m_top + (Fl::event_y() < y() ? -1 : 1));
                    sync_bar();
                }
                const int ey = Fl::event_y() < y() + 2 ? y() + 2
                             : Fl::event_y() >= y() + h() - 4 ? y() + 2 + (visible_rows() - 1) * data_line_height()
                             : Fl::event_y();
                const int a = addr_at(Fl::event_x(), ey);
                if (a >= 0 && a != m_sel) {
                    m_sel = a;
                    selection_changed();
                    redraw();
                }
                return 1;
            }
            case FL_RELEASE:
                m_dragging = false;
                return 1;
            case FL_FOCUS:
            case FL_UNFOCUS:
                m_nibble = -1;
                redraw();
                return 1;
            case FL_MOUSEWHEEL:
                m_top += Fl::event_dy() * 3;
                clamp_top();
                sync_bar();
                redraw();
                return 1;
            case FL_KEYBOARD:
                return key(Fl::event_key(), Fl::event_text());
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        clamp_top();
        sync_bar();
    }

private:
    int hex_offset(int c) const { return c * m_cw * 3 + (c >= 8 ? m_cw : 0); }

    void geometry(int cw)
    {
        m_cw = cw;
        m_hex_x = x() + 6 + cw * 6;
        m_ascii_x = m_hex_x + hex_offset(COLS) + cw;
    }

    void selection_changed()
    {
        if (on_selection) on_selection();
    }

    /** Text-column glyph of a byte (UTF-8). */
    void glyph(uint8_t b, char* ch) const
    {
        if (cp437) {
            const unsigned cp = s_cp437[b] ? s_cp437[b] : ' ';
            const int n = fl_utf8encode(cp, ch);
            ch[n] = '\0';
        } else {
            ch[0] = isprint(b) ? (char) b : '.';
            ch[1] = '\0';
        }
    }

    /** Flag bytes of [start, start+len) covered by a search-pattern match. */
    void mark_matches(int start, int len, bool* out)
    {
        const int n = m_find.size();
        bool any = false;
        for (uint8_t m : m_find.mask) any = any || m;
        if (n == 0 || !any) return;
        const int lo = start - (n - 1) < 0 ? 0 : start - (n - 1);
        const int hi = start + len + (n - 1) > 0x10000 ? 0x10000 : start + len + (n - 1);
        std::vector<uint8_t> buf((size_t) (hi - lo));
        debugger_read_memory(fz_debugger(), (hwaddr) lo, hi - lo, buf.data());
        for (int a = lo; a + n <= hi; a++) {
            if (!m_find.matches(&buf[(size_t) (a - lo)])) continue;
            for (int i = a; i < a + n; i++) {
                if (i >= start && i < start + len) out[i - start] = true;
            }
        }
    }

    std::vector<uint8_t> selected_bytes() const
    {
        std::vector<uint8_t> data((size_t) sel_len());
        if (!data.empty()) debugger_read_memory(fz_debugger(), (hwaddr) sel_lo(), (int) data.size(), data.data());
        return data;
    }

    static void copy_text(const std::string& text)
    {
        Fl::copy(text.c_str(), (int) text.size(), 1);
    }

    void copy_hex() const
    {
        std::string text;
        char b[8];
        for (uint8_t v : selected_bytes()) {
            hex8(b, sizeof(b), v);
            if (!text.empty()) text += ' ';
            text += b;
        }
        copy_text(text);
    }

    void context_menu()
    {
        const int lo = sel_lo();
        const int hi = sel_hi();
        const bool range = hi > lo;
        Fl_Menu_Item items[] = {
            { "Copy Address", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { range ? "Copy Bytes as Hex" : "Copy Byte as Hex", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Copy as Text", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Find...", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Find Changed Bytes...", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Fill Selection...", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Save Range to File...", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Load File into Memory...", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Go to Address...", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Show in Disassembly", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        };
        const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
        if (picked == nullptr) return;
        const memtools::Writer writer = [this](unsigned addr, const std::vector<uint8_t>& data) {
            write(addr, data);
        };
        char text[8];
        switch ((int) (picked - items)) {
            case 0:
                hex16(text, sizeof(text), (unsigned) lo);
                copy_text(text);
                break;
            case 1:
                copy_hex();
                break;
            case 2: {
                std::string out;
                char ch[8];
                for (uint8_t v : selected_bytes()) {
                    glyph(v, ch);
                    out += ch;
                }
                copy_text(out);
                break;
            }
            case 3:
                if (on_find) on_find();
                break;
            case 4:
                if (on_changes) on_changes();
                break;
            case 5:
                memtools::fill_dialog((unsigned) lo, (unsigned) hi, writer);
                break;
            case 6:
                memtools::save_range_dialog((unsigned) lo, (unsigned) hi);
                break;
            case 7:
                memtools::load_file_dialog((unsigned) lo, writer);
                break;
            case 8: {
                unsigned addr;
                if (memtools::address_dialog("GO TO ADDRESS", (unsigned) lo, &addr) && on_goto) on_goto((hwaddr) addr);
                break;
            }
            case 9:
                goto_disassembly((hwaddr) lo);
                break;
            default:
                break;
        }
        redraw();
    }

    int addr_at(int ex, int ey)
    {
        const int lh = data_line_height();
        geometry(char_width());
        const int r = (ey - y() - 2) / lh;
        if (ey < y() + 2 || r >= visible_rows()) return -1;
        int c = -1;
        for (int i = 0; i < COLS; i++) {
            const int hx = m_hex_x + hex_offset(i);
            if (ex >= hx - 2 && ex < hx + m_cw * 2 + 2) c = i;
            if (ex >= m_ascii_x + i * m_cw && ex < m_ascii_x + (i + 1) * m_cw) c = i;
        }
        if (c < 0) return -1;
        const int a = (m_top + r) * COLS + c;
        return a < 0x10000 ? a : -1;
    }

    int key(int k, const char* text)
    {
        const int state = Fl::event_state();
        if (state & FL_COMMAND) {
            if (k == 'f') {
                if (on_find) on_find();
                return 1;
            }
            if (k == 'c' && m_sel >= 0) {
                copy_hex();
                return 1;
            }
            return 0;
        }
        if (m_sel < 0) return 0;
        int move = 0;
        switch (k) {
            case FL_Left:  move = -1; break;
            case FL_Right: move = 1; break;
            case FL_Up:    move = -COLS; break;
            case FL_Down:  move = COLS; break;
            case FL_Page_Up:   move = -COLS * (visible_rows() - 1); break;
            case FL_Page_Down: move = COLS * (visible_rows() - 1); break;
            case FL_Escape:
                m_nibble = -1;
                if (m_anchor != m_sel) {
                    m_anchor = m_sel;
                    selection_changed();
                }
                redraw();
                return 1;
            default: break;
        }
        if (move) {
            int a = m_sel + move;
            if (a < 0) a = 0;
            if (a > 0xFFFF) a = 0xFFFF;
            go((hwaddr) a, true, (state & FL_SHIFT) != 0);
            return 1;
        }
        if (state & (FL_CTRL | FL_ALT | FL_META)) return 0;
        if (text && isxdigit((unsigned char) text[0]) && text[1] == '\0') {
            const int d = isdigit((unsigned char) text[0]) ? text[0] - '0' : (tolower(text[0]) - 'a' + 10);
            if (m_nibble < 0) {
                m_nibble = d;
            } else {
                uint8_t value = (uint8_t) ((m_nibble << 4) | d);
                debugger_write_memory(fz_debugger(), (hwaddr) m_sel, 1, &value);
                m_snapshot[m_sel] = value;
                m_nibble = -1;
                if (m_sel < 0xFFFF) go((hwaddr) (m_sel + 1));
                request_refresh();
            }
            redraw();
            return 1;
        }
        return 0;
    }

    void clamp_top()
    {
        const int rows = visible_rows();
        if (m_top > ROWS_TOTAL - rows) m_top = ROWS_TOTAL - rows;
        if (m_top < 0) m_top = 0;
    }

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
    memtools::Pattern m_find;
    std::vector<uint8_t> m_prev;
    std::vector<uint8_t> m_snapshot;
    bool m_have_prev = false;
    bool m_have_snapshot = false;
};

/** Search field of the memory panel: Enter = next, Shift+Enter = previous,
 *  Escape closes the search row. */
class MemFindInput : public Fl_Input {
public:
    MemFindInput(int X, int Y, int W, int H) : Fl_Input(X, Y, W, H) {}

    std::function<void(bool forward)> on_enter;
    std::function<void()> on_escape;

    int handle(int event) override
    {
        if (event == FL_KEYBOARD) {
            const int k = Fl::event_key();
            if ((k == FL_Enter || k == FL_KP_Enter) && on_enter) {
                on_enter((Fl::event_state() & FL_SHIFT) == 0);
                return 1;
            }
            if (k == FL_Escape && on_escape) {
                on_escape();
                return 1;
            }
        }
        return Fl_Input::handle(event);
    }
};

class MemoryPanel : public Panel {
public:
    /** Width that shows a full 16-byte row with its ASCII column. */
    static int preferred_width()
    {
        const int cw = char_width();
        return 6 + cw * 6 + (16 * 3 + 1) * cw + cw + 16 * cw + 8 + 14;
    }

    MemoryPanel(int X, int Y, int W, int H) : Panel(X, Y, W, H, "MEMORY")
    {
        m_input = new Fl_Input(X, Y, 10, 10);
        style_input(m_input);
        m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
        m_input->callback(cb_go, this);
        m_input->tooltip("Address (hex) or symbol name, then Enter\nPart of a name lists the matching symbols");
        m_follow = new Fl_Choice(X, Y, 10, 10);
        m_follow->add("Free|PC|SP|HL|DE|BC|IX|IY");
        m_follow->value(0);
        m_follow->textsize(12);
        m_follow->clear_visible_focus();
        m_follow->tooltip("Keep the view on the address held by a register");
        m_follow->callback(cb_follow, this);
        m_cp437 = new Fl_Check_Button(X, Y, 10, 10, "CP437");
        style_check(m_cp437);
        m_cp437->callback(cb_cp437, this);
        m_find_btn = new Fl_Button(X, Y, 10, 10, "Find");
        style_button(m_find_btn);
        m_find_btn->tooltip("Search hex bytes (3E 22 ?\?) or a \"string\" (Cmd/Ctrl+F in the view)");
        m_find_btn->callback(cb_find_toggle, this);
        m_changes_btn = new Fl_Button(X, Y, 10, 10, "Changes");
        style_button(m_changes_btn);
        m_changes_btn->tooltip("Find bytes that changed between pauses (cheat-finder style)");
        m_changes_btn->callback(cb_changes, this);

        m_find = new MemFindInput(X, Y, 10, 10);
        style_input(m_find);
        m_find->when(FL_WHEN_CHANGED);
        m_find->callback(cb_find_changed, this);
        m_find->tooltip("Hex bytes with ?? wildcards (3E 22 ?\?) or a quoted string (\"HELLO\").\n"
                        "Enter: next, Shift+Enter: previous, Escape: close");
        m_find->on_enter = [this](bool forward) { find(forward); };
        m_find->on_escape = [this]() { show_find(false); };
        m_prev_btn = new Fl_Button(X, Y, 10, 10, "@-2<");
        style_button(m_prev_btn);
        m_prev_btn->tooltip("Previous match (Shift+Enter)");
        m_prev_btn->callback(cb_find_prev, this);
        m_next_btn = new Fl_Button(X, Y, 10, 10, "@-2>");
        style_button(m_next_btn);
        m_next_btn->tooltip("Next match (Enter)");
        m_next_btn->callback(cb_find_next, this);
        m_close_btn = new Fl_Button(X, Y, 10, 10, "x");
        style_button(m_close_btn);
        m_close_btn->tooltip("Close search (Escape)");
        m_close_btn->callback(cb_find_toggle, this);
        m_find_status = new Fl_Box(X, Y, 10, 10);
        m_find_status->labelsize(12);
        m_find_status->labelcolor(COLOR_DIM);
        m_find_status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

        m_mmu = new MmuStrip(X, Y, 10, 10);
        m_bar = new Fl_Scrollbar(X, Y, 10, 10);
        m_bar->type(FL_VERTICAL);
        m_bar->linesize(1);
        m_bar->color(COLOR_SURFACE);
        m_bar->callback(cb_bar, this);
        m_view = new MemView(X, Y, 10, 10, m_bar);
        m_view->on_selection = [this]() { update_extra(); };
        m_view->on_find = [this]() { show_find(true); };
        m_view->on_changes = [this]() { show_changes(); };
        m_view->on_goto = [this](hwaddr addr) { go(addr); };
        resizable(m_view);
        end();
        show_find(false);
        resize(X, Y, W, H);
    }

    ~MemoryPanel() override { memtools::change_search_destroy(); }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        const int by = Y + HEADER_H + 4;
        m_input->resize(X + 6, by, 130, BAR_H - 8);
        m_follow->resize(X + 142, by, 74, BAR_H - 8);
        m_cp437->resize(X + 224, by, 74, BAR_H - 8);
        m_find_btn->resize(X + 300, by, 52, BAR_H - 8);
        m_changes_btn->resize(X + 356, by, 74, BAR_H - 8);
        int my = Y + HEADER_H + BAR_H;
        if (m_find_shown) {
            m_find->resize(X + 6, my, 210, BAR_H - 8);
            m_prev_btn->resize(X + 220, my, 26, BAR_H - 8);
            m_next_btn->resize(X + 248, my, 26, BAR_H - 8);
            m_close_btn->resize(X + 276, my, 22, BAR_H - 8);
            m_find_status->resize(X + 306, my, W - 312, BAR_H - 8);
            my += BAR_H - 4;
        }
        const int mh = 26;
        m_mmu->resize(X, my, W, mh);
        m_bar->resize(X + W - 14, my + mh, 14, H - (my + mh - Y));
        m_view->resize(X, my + mh, W - 14, H - (my + mh - Y));
    }

    void refresh(bool paused, bool force) override
    {
        if (paused && force) {
            /* Snapshot only on a real pause (registers moved; R changes with
             * every instruction), not on refreshes after user actions. */
            const regs_t r = exact_regs();
            if (!m_was_paused || memcmp(&r, &m_pause_regs, sizeof(r)) != 0) {
                m_view->snapshot();
                m_pause_regs = r;
                memtools::change_search_paused();
            }
        }
        m_was_paused = paused;
        m_mmu->refresh();
        const int follow = m_follow->value();
        if (follow > 0) {
            const regs_t r = current_regs();
            const uint16_t values[] = { 0, r.pc, r.sp, r.hl, r.de, r.bc, r.ix, r.iy };
            m_view->follow_addr = values[follow];
            m_view->go(values[follow], false);
        } else {
            m_view->follow_addr = -1;
        }
        update_extra();
        m_view->redraw();
    }


    void go(hwaddr addr)
    {
        set_input(addr);
        unfollow();
        m_view->go(addr);
    }

private:
    void set_input(hwaddr addr)
    {
        char text[8];
        hex16(text, sizeof(text), addr);
        m_input->value(text);
    }

    void unfollow()
    {
        m_follow->value(0);
        m_view->follow_addr = -1;
        update_extra();
    }

    /** Header: the selection range, else the followed register. */
    void update_extra()
    {
        char text[48] = "";
        if (m_view->sel_len() > 1) {
            char lo[8], hi[8];
            hex16(lo, sizeof(lo), (unsigned) m_view->sel_lo());
            hex16(hi, sizeof(hi), (unsigned) m_view->sel_hi());
            snprintf(text, sizeof(text), "SEL $%s-$%s (%d)", lo, hi, m_view->sel_len());
        } else if (m_follow->value() > 0) {
            snprintf(text, sizeof(text), "FOLLOW %s", m_follow->text());
        }
        extra(text);
    }

    void show_find(bool show)
    {
        m_find_shown = show;
        Fl_Widget* row[] = { m_find, m_prev_btn, m_next_btn, m_close_btn, m_find_status };
        for (Fl_Widget* wd : row) {
            if (show) wd->show();
            else wd->hide();
        }
        resize(x(), y(), w(), h());
        if (show) {
            m_find->take_focus();
#if FL_API_VERSION >= 10400
            m_find->insert_position(m_find->size(), 0);
#else
            m_find->position(m_find->size(), 0);
#endif
            cb_find_changed(nullptr, this);
        } else {
            m_view->highlight(memtools::Pattern());
            if (Fl::focus() == m_find) m_view->take_focus();
        }
        redraw();
    }

    void find_status(const char* text, Fl_Color c)
    {
        m_find_status->copy_label(text);
        m_find_status->labelcolor(c);
        m_find_status->redraw();
    }

    /** Search from the selection: Enter on a new query starts at the cursor. */
    void find(bool forward)
    {
        if (!dbg_ready()) return;
        memtools::Pattern p;
        std::string err;
        if (!memtools::parse_pattern(m_find->value(), p, &err)) {
            find_status(err.c_str(), COLOR_BREAK);
            return;
        }
        std::vector<uint8_t> mem(0x10000);
        debugger_read_memory(fz_debugger(), 0, 0x10000, mem.data());
        const bool fresh = m_last_query != m_find->value();
        m_last_query = m_find->value();
        const int lo = m_view->sel_len() > 0 ? m_view->sel_lo() : m_view->top_addr();
        const int from = forward ? (fresh ? lo : lo + 1) : lo - 1;
        bool wrapped = false;
        const int a = memtools::find(mem.data(), 0x10000, p, from, forward, &wrapped);
        m_view->highlight(p);
        if (a < 0) {
            find_status("No match", COLOR_BREAK);
            return;
        }
        unfollow();
        m_view->select_range(a, a + p.size() - 1);
        set_input((hwaddr) a);
        char ad[8], text[48];
        hex16(ad, sizeof(ad), (unsigned) a);
        snprintf(text, sizeof(text), "Found at $%s%s", ad, wrapped ? "  (wrapped)" : "");
        find_status(text, wrapped ? COLOR_AMBER : COLOR_GREEN);
    }

    void show_changes()
    {
        memtools::SnapshotSource src;
        MemView* view = m_view;
        src.get = [view](const uint8_t** prev, const uint8_t** cur) { return view->snapshots(prev, cur); };
        src.go = [this](unsigned addr) { go((hwaddr) addr); };
        memtools::change_search_show(src);
    }

    static void cb_go(Fl_Widget*, void* data)
    {
        MemoryPanel* self = static_cast<MemoryPanel*>(data);
        hwaddr addr;
        if (lookup_address(self->m_input, self->m_input->value(), &addr, SYM_KIND_DATA)) self->go(addr);
        else fl_beep();
    }

    static void cb_follow(Fl_Widget*, void* data)
    {
        static_cast<MemoryPanel*>(data)->refresh(fz_paused(), false);
    }

    static void cb_cp437(Fl_Widget*, void* data)
    {
        MemoryPanel* self = static_cast<MemoryPanel*>(data);
        self->m_view->cp437 = self->m_cp437->value() != 0;
        self->m_view->redraw();
    }

    static void cb_bar(Fl_Widget*, void* data)
    {
        MemoryPanel* self = static_cast<MemoryPanel*>(data);
        self->m_view->scroll_to_row(self->m_bar->value());
    }

    static void cb_find_toggle(Fl_Widget*, void* data)
    {
        MemoryPanel* self = static_cast<MemoryPanel*>(data);
        self->show_find(!self->m_find_shown);
    }

    /** Live: highlight matches of the query as it is typed. */
    static void cb_find_changed(Fl_Widget*, void* data)
    {
        MemoryPanel* self = static_cast<MemoryPanel*>(data);
        memtools::Pattern p;
        if (!memtools::parse_pattern(self->m_find->value(), p, nullptr)) p = memtools::Pattern();
        self->m_view->highlight(p);
        self->find_status(self->m_find->size() ? "Enter: next  Shift+Enter: previous" : "", COLOR_DIM);
    }

    static void cb_find_next(Fl_Widget*, void* data) { static_cast<MemoryPanel*>(data)->find(true); }
    static void cb_find_prev(Fl_Widget*, void* data) { static_cast<MemoryPanel*>(data)->find(false); }
    static void cb_changes(Fl_Widget*, void* data) { static_cast<MemoryPanel*>(data)->show_changes(); }

    Fl_Input* m_input;
    Fl_Choice* m_follow;
    Fl_Check_Button* m_cp437;
    Fl_Button* m_find_btn;
    Fl_Button* m_changes_btn;
    MemFindInput* m_find;
    Fl_Button* m_prev_btn;
    Fl_Button* m_next_btn;
    Fl_Button* m_close_btn;
    Fl_Box* m_find_status;
    MmuStrip* m_mmu;
    Fl_Scrollbar* m_bar;
    MemView* m_view;
    bool m_find_shown = false;
    std::string m_last_query;
    bool m_was_paused = false;
    regs_t m_pause_regs = {};
};

/* ------------------------------------------------------------------ */
/*  Debugger                                                           */
/* ------------------------------------------------------------------ */

constexpr int DEFAULT_LEFT_W = FZ_SCREEN_WIDTH;
constexpr int DEFAULT_SCREEN_H = FZ_SCREEN_HEIGHT;
constexpr int DEFAULT_RIGHT_W = 300;
constexpr int DEFAULT_BREAKPOINTS_H = 150;
constexpr int DEFAULT_WATCH_H = 170;
constexpr int DEFAULT_DISASM_W = 420;
constexpr int DEFAULT_MEMORY_H = 300;

static int default_left_w();
LayoutConfig g_layout;

static void layout_load()
{
    g_layout.left_w = config_get("FLTK_DBG_LEFT_WIDTH", 0);
    g_layout.screen_h = config_get("FLTK_DBG_SCREEN_HEIGHT", 0);
    g_layout.right_w = config_get("FLTK_DBG_RIGHT_WIDTH", 0);
    g_layout.breakpoints_h = config_get("FLTK_DBG_BREAKPOINTS_HEIGHT", 0);
    g_layout.watch_h = config_get("FLTK_DBG_WATCH_HEIGHT", 0);
    g_layout.video_x = config_get("FLTK_VIDEO_X", -1);
    g_layout.video_y = config_get("FLTK_VIDEO_Y", -1);
    g_layout.video_w = config_get("FLTK_VIDEO_WIDTH", 0);
    g_layout.video_h = config_get("FLTK_VIDEO_HEIGHT", 0);
    g_layout.video_shown = config_get("FLTK_VIDEO_SHOWN", 0) != 0;
}

static void layout_save(rini_config* ini)
{
    const LayoutConfig& c = g_layout;
    rini_set_config_comment_line(ini, "SDL/FLTK debugger layout");
    rini_set_config_value(ini, "FLTK_DBG_LEFT_WIDTH", c.left_w, "Screen/memory column width");
    rini_set_config_value(ini, "FLTK_DBG_SCREEN_HEIGHT", c.screen_h, "Screen height");
    rini_set_config_value(ini, "FLTK_DBG_RIGHT_WIDTH", c.right_w, "CPU/stack column width");
    rini_set_config_value(ini, "FLTK_DBG_BREAKPOINTS_HEIGHT", c.breakpoints_h, "Breakpoints height");
    rini_set_config_value(ini, "FLTK_DBG_WATCH_HEIGHT", c.watch_h, "Watch height");
    rini_set_config_value(ini, "FLTK_VIDEO_X", c.video_x, "Video window X");
    rini_set_config_value(ini, "FLTK_VIDEO_Y", c.video_y, "Video window Y");
    rini_set_config_value(ini, "FLTK_VIDEO_WIDTH", c.video_w, "Video window width");
    rini_set_config_value(ini, "FLTK_VIDEO_HEIGHT", c.video_h, "Video window height");
    rini_set_config_value(ini, "FLTK_VIDEO_SHOWN", c.video_shown, "Video window open");
}

struct Debugger::Impl {
    DockLayout* dock = nullptr;
    ScreenView* screen = nullptr;
    Toolbar* toolbar = nullptr;
    SplitPane* root = nullptr;   /* left | rest */
    SplitPane* left = nullptr;   /* screen / memory */
    SplitPane* rest = nullptr;   /* disassembly | right */
    SplitPane* right = nullptr;  /* registers / lower */
    SplitPane* lower = nullptr;  /* stack / tail */
    SplitPane* tail = nullptr;   /* watch / breakpoints */
    Panel* watch = nullptr;
    RegistersView* regs = nullptr;
    StackView* stack = nullptr;
    BreakpointsPanel* breakpoints = nullptr;
    DisasmPanel* disasm = nullptr;
    MemoryPanel* memory = nullptr;
    double last_refresh = 0;

    void refresh(bool force)
    {
        if (!dbg_ready() || toolbar == nullptr) return;
        const bool paused = fz_paused();
        const double now = now_seconds();
        if (!force && (paused || now - last_refresh < 0.1)) return;
        last_refresh = now;
        toolbar->update(paused, current_regs().pc);
        regs->refresh(paused, force);
        stack->refresh(paused, force);
        watch->refresh(paused, force);
        breakpoints->refresh(paused, force);
        disasm->refresh(paused, force);
        memory->refresh(paused, force);
        video_window_refresh(paused, force);
        tool_windows_refresh(paused, force);
    }
};

void request_refresh()
{
    if (s_impl) s_impl->refresh(true);
    if (main_window()) main_window()->update_status();
}

void goto_memory(hwaddr addr)
{
    if (s_impl) s_impl->memory->go(addr);
}

void goto_disassembly(hwaddr addr)
{
    if (s_impl) s_impl->disasm->go(addr);
}

void Toolbar::cb_run_to(Fl_Widget*, void*)
{
    if (!s_impl || !dbg_ready() || !fz_paused()) return;
    const int addr = s_impl->disasm->selected();
    if (addr < 0) {
        notif_show("Select a line in the disassembly first");
        return;
    }
    debugger_set_temporary_breakpoint(fz_debugger(), (hwaddr) addr);
    debugger_continue(fz_debugger());
    request_refresh();
}

Debugger::Debugger(DockLayout* dock, ScreenView* screen) : m(new Impl)
{
    s_impl = m;
    m->dock = dock;
    m->screen = screen;
    layout_load();
    persist_register(layout_save);

    dock->begin();
    m->root = new SplitPane(0, 0, 1400, 800, true, false);
    m->root->end();
    m->left = new SplitPane(0, 0, default_left_w(), 800, false, false);
    m->left->end();
    m->memory = new MemoryPanel(0, 0, default_left_w(), DEFAULT_MEMORY_H);
    m->memory->end();
    m->rest = new SplitPane(0, 0, 700, 800, true, true);
    m->rest->end();
    m->disasm = new DisasmPanel(0, 0, DEFAULT_DISASM_W, 800);
    m->disasm->end();
    m->right = new SplitPane(0, 0, DEFAULT_RIGHT_W, 800, false, false);
    m->right->end();
    m->regs = new RegistersView(0, 0, DEFAULT_RIGHT_W, RegistersView::preferred_height());
    m->regs->end();
    m->lower = new SplitPane(0, 0, DEFAULT_RIGHT_W, 400, false, true);
    m->lower->end();
    m->stack = new StackView(0, 0, DEFAULT_RIGHT_W, 200);
    m->stack->end();
    m->tail = new SplitPane(0, 0, DEFAULT_RIGHT_W, 300, false, true);
    m->tail->end();
    m->watch = create_watch_panel(0, 0, DEFAULT_RIGHT_W, DEFAULT_WATCH_H);
    m->watch->end();
    m->breakpoints = new BreakpointsPanel(0, 0, DEFAULT_RIGHT_W, DEFAULT_BREAKPOINTS_H);
    m->breakpoints->end();
    dock->end();

    m->left->set(screen, m->memory, DEFAULT_SCREEN_H, 120, 140);
    m->tail->set(m->watch, m->breakpoints, DEFAULT_BREAKPOINTS_H, 60, 90);
    m->lower->set(m->stack, m->tail, DEFAULT_WATCH_H + DEFAULT_BREAKPOINTS_H + SplitPane::GAP, 60, 160);
    m->right->set(m->regs, m->lower, RegistersView::preferred_height(), 120, 150);
    m->rest->set(m->disasm, m->right, DEFAULT_RIGHT_W, 260, 240);
    m->root->set(m->left, m->rest, default_left_w(), MemoryPanel::preferred_width(), 520);

    Impl* impl = m;
    dock->set_root(m->root, [impl](bool expanded) {
        impl->root->collapse(!expanded);
        impl->left->collapse(!expanded);
        if (!expanded) {
            video_window_suspend();
            tool_windows_suspend();
        }
    });
}

Debugger::~Debugger()
{
    video_window_destroy();
    tool_windows_destroy();
    s_impl = nullptr;
    delete m;
}

Fl_Group* Debugger::toolbar(int x, int y, int w, int h)
{
    m->toolbar = new Toolbar(x, y, w, h);
    return m->toolbar;
}

void Debugger::refresh(bool force) { m->refresh(force); }
void Debugger::goto_memory(hwaddr addr) { zui::goto_memory(addr); }
void Debugger::goto_disassembly(hwaddr addr) { zui::goto_disassembly(addr); }

static int default_left_w()
{
    const int pref = MemoryPanel::preferred_width();
    return pref > DEFAULT_LEFT_W ? pref : DEFAULT_LEFT_W;
}

/* The screen fills the column width at 4:3; memory gets the rest */
static int default_screen_h(int left_w)
{
    const int body_h = DEFAULT_SCREEN_H + DEFAULT_MEMORY_H;
    const int fill = left_w * FZ_SCREEN_HEIGHT / FZ_SCREEN_WIDTH;
    const int min_memory = 160;
    return std::max(DEFAULT_SCREEN_H, std::min(fill, body_h - min_memory));
}

void Debugger::default_body_size(int& w, int& h) const
{
    w = default_left_w() + DEFAULT_DISASM_W + DEFAULT_RIGHT_W + 2 * SplitPane::GAP;
    h = DEFAULT_SCREEN_H + DEFAULT_MEMORY_H + SplitPane::GAP;
}

void Debugger::apply_layout()
{
    const LayoutConfig& c = g_layout;
    const int bps_h = c.breakpoints_h > 0 ? c.breakpoints_h : DEFAULT_BREAKPOINTS_H;
    const int watch_h = c.watch_h > 0 ? c.watch_h : DEFAULT_WATCH_H;
    const int left_w = c.left_w > 0 ? c.left_w : default_left_w();
    m->root->fixed_size(left_w);
    m->left->fixed_size(c.screen_h > 0 ? c.screen_h : default_screen_h(left_w));
    m->rest->fixed_size(c.right_w > 0 ? c.right_w : DEFAULT_RIGHT_W);
    m->lower->fixed_size(watch_h + bps_h + SplitPane::GAP);
    m->tail->fixed_size(bps_h);
    if (m->dock->docks_visible()) {
        /* Open tool windows once the main window is on screen, so they float above it */
        Fl::add_timeout(0.3, [](void*) {
            if (main_window() && main_window()->shown() && main_window()->debug_layout()) {
                if (g_layout.video_shown) video_window_show(true);
                tool_windows_resume();
                main_window()->update_menus();
            }
        });
    }
}

void Debugger::capture_layout()
{
    if (!m->dock->docks_visible()) return;
    g_layout.left_w = m->root->fixed_size();
    g_layout.screen_h = m->left->fixed_size();
    g_layout.right_w = m->rest->fixed_size();
    g_layout.breakpoints_h = m->tail->fixed_size();
    g_layout.watch_h = m->lower->fixed_size() - m->tail->fixed_size() - SplitPane::GAP;
    video_window_capture_config();
}

void Debugger::reset_layout()
{
    g_layout = LayoutConfig();
    video_window_show(false);
    tool_windows_reset();
    apply_layout();
}

void Debugger::show_video_window(bool show)
{
    video_window_show(show);
    if (show) m->refresh(true);
}

bool Debugger::video_window_shown() const
{
    return zui::video_window_shown();
}

#else /* !CONFIG_ENABLE_DEBUGGER */

struct Debugger::Impl {};
Debugger::Debugger(DockLayout* dock, ScreenView* screen) : m(nullptr) { dock->set_root(screen, nullptr); }
Debugger::~Debugger() { delete m; }
Fl_Group* Debugger::toolbar(int x, int y, int w, int h) { return new Fl_Group(x, y, w, h); }
void Debugger::refresh(bool) {}
void Debugger::goto_memory(hwaddr) {}
void Debugger::goto_disassembly(hwaddr) {}
void Debugger::default_body_size(int& w, int& h) const { w = FZ_SCREEN_WIDTH; h = FZ_SCREEN_HEIGHT; }
void Debugger::apply_layout() {}
void Debugger::capture_layout() {}
void Debugger::reset_layout() {}
void Debugger::show_video_window(bool) {}
bool Debugger::video_window_shown() const { return false; }

#endif

} // namespace zui
