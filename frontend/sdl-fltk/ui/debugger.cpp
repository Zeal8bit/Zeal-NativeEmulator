/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Debugger layout. Everything needed while stepping stays visible together,
 * no tabs (in the spirit of Mesen and no$sns):
 *
 *   +---------------------------------------------------------------+
 *   | toolbar: run/pause  step  over  run-to | break  reset | state  |
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
#include "ui.h"
#include "video.h"

#if CONFIG_ENABLE_DEBUGGER
extern "C" {
#include "debugger/zeal_debugger.h"
}
#endif

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

constexpr int BAR_H = 30;

static Debugger::Impl* s_impl;
static void request_refresh();
static void goto_memory(hwaddr addr);
static void goto_disassembly(hwaddr addr);

static void hex16(char* out, size_t size, unsigned value)
{
    snprintf(out, size, hex16_fmt(), value & 0xFFFF);
}

static void hex8(char* out, size_t size, unsigned value)
{
    snprintf(out, size, hex8_fmt(), value & 0xFF);
}

static int char_width()
{
    fl_font(DATA_FONT, DATA_SIZE);
    return (int) fl_width("0");
}

static int text_baseline(int line_h)
{
    fl_font(DATA_FONT, DATA_SIZE);
    return line_h - fl_descent() - (line_h - fl_height()) / 2;
}

static void style_input(Fl_Input* in)
{
    in->textfont(DATA_FONT);
    in->textsize(DATA_SIZE);
    in->color(COLOR_BG);
    in->textcolor(COLOR_TEXT);
    in->cursor_color(COLOR_AMBER);
    in->selection_color(COLOR_SELECT);
    in->box(FL_BORDER_BOX);
}

static void style_button(Fl_Button* b)
{
    b->labelsize(12);
    b->box(FL_FLAT_BOX);
    b->down_box(FL_FLAT_BOX);
    b->color(COLOR_RAISED);
    b->selection_color(COLOR_BORDER);
    b->labelcolor(COLOR_TEXT);
    b->clear_visible_focus();
}

static void style_check(Fl_Check_Button* b)
{
    b->labelsize(12);
    b->labelcolor(COLOR_TEXT);
    b->selection_color(COLOR_AMBER);
    b->clear_visible_focus();
}

static bool dbg_ready()
{
    return fz_debugger() != nullptr;
}

/** Registers to display: exact when paused, the mid-frame sample while
 *  running (reading them here would always land on the frame boundary). */
static regs_t current_regs()
{
    regs_t regs;
    memset(&regs, 0, sizeof(regs));
    if (!dbg_ready()) return regs;
    if (!fz_paused() && fz_live_sample(&regs, nullptr, 0, nullptr, 0)) return regs;
    debugger_get_registers(fz_debugger(), &regs);
    return regs;
}

/** Registers for editing: always the exact current state. */
static regs_t exact_regs()
{
    regs_t regs;
    memset(&regs, 0, sizeof(regs));
    if (dbg_ready()) debugger_get_registers(fz_debugger(), &regs);
    return regs;
}

/** Memory at the stack/HL as displayed: sampled while running. */
static void display_memory(hwaddr addr, int len, uint8_t* out, bool stack)
{
    if (!fz_paused()) {
        const bool ok = stack ? fz_live_sample(nullptr, out, len, nullptr, 0)
                              : fz_live_sample(nullptr, nullptr, 0, out, len);
        if (ok) return;
    }
    debugger_read_memory(fz_debugger(), addr, len, out);
}

/* ------------------------------------------------------------------ */
/*  Panel: surface with a retro header strip                           */
/* ------------------------------------------------------------------ */

class Panel : public Fl_Group {
public:
    Panel(int X, int Y, int W, int H, const char* title)
        : Fl_Group(X, Y, W, H), m_title(title)
    {
        box(FL_FLAT_BOX);
        color(COLOR_SURFACE);
    }

    void extra(const char* text)
    {
        if (strcmp(m_extra, text) == 0) return;
        snprintf(m_extra, sizeof(m_extra), "%s", text);
        damage(FL_DAMAGE_ALL, x(), y(), w(), HEADER_H);
    }

    void draw() override
    {
        Fl_Group::draw();
        draw_header(x(), y(), w(), m_title, m_extra);
    }

    int top() const { return y() + HEADER_H; }
    const char* extra_text() const { return m_extra; }
    virtual void refresh(bool paused, bool force) = 0;

private:
    const char* m_title;
    char m_extra[64] = "";
};

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
        IconButton* bp = make(Icon::Breakpoint, "Break", 78, cb_breakpoint, "Toggle a breakpoint at PC (Cmd/Ctrl+F9)");
        bp->icon_color(COLOR_BREAK);
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
    static void cb_breakpoint(Fl_Widget*, void*)
    {
        if (!dbg_ready()) return;
        debugger_breakpoint(fz_debugger());
        request_refresh();
    }
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
        return HEADER_H + 8 + 7 * lh + 22 + lh + 4 + 12 + 2 * lh + 10;
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
            if (id == m_hover && m_paused) {
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

        /* What HL and SP point at */
        int py = fy + lh + 4 + 12;
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
        fl_color(COLOR_CYAN);
        fl_draw("(SP)", x() + 10, py + asc);
        hex16(text, sizeof(text), top_word);
        fl_color(COLOR_TEXT);
        fl_draw(text, x() + 10 + cw * 5, py + asc);
        const char* sym = dbg_ready() ? debugger_get_symbol(fz_debugger(), top_word) : nullptr;
        if (sym) {
            fl_color(COLOR_AMBER);
            fl_draw(sym, x() + 10 + cw * 10, py + asc);
        }

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
                const int id = hit_value(Fl::event_x(), Fl::event_y());
                if (id != m_hover) {
                    m_hover = id;
                    redraw();
                    tooltip_for(id);
                }
                return 1;
            }
            case FL_PUSH: {
                if (m_edit->visible() && Fl::event_inside(m_edit)) break;
                const int ex = Fl::event_x(), ey = Fl::event_y();
                const int name = hit_name(ex, ey);
                if (name >= 0 && !reg_is_8bit(name)) {
                    goto_memory(reg_get(m_regs, name));
                    return 1;
                }
                const int id = hit_value(ex, ey);
                if (id >= 0 && m_paused) {
                    begin_edit(id);
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
            snprintf(tip, sizeof(tip), "%s = %u", s_reg_names[id], v);
        } else {
            const int hi = (v >> 8) & 0xFF, lo = v & 0xFF;
            snprintf(tip, sizeof(tip), "%s = %u (signed %d)\nhigh %u '%c'   low %u '%c'%s",
                     s_reg_names[id], v, (int) (int16_t) v, hi, isprint(hi) ? hi : '.', lo,
                     isprint(lo) ? lo : '.', m_paused ? "\nClick to edit" : "");
        }
        int nx, vx, ry;
        cell(id, nx, vx, ry);
        Fl_Tooltip::enter_area(this, vx - x(), ry - y(), char_width() * 4, data_line_height(), tip);
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
        tooltip("Click: show in memory    Double-click: show the word in the disassembly");
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        m_regs = current_regs();
        char text[24];
        char sp[8];
        hex16(sp, sizeof(sp), m_regs.sp);
        snprintf(text, sizeof(text), "SP $%s", sp);
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
        int count = rows < 128 ? rows : 128;
        if (!fz_paused() && count > 32) count = 32; /* size of the live sample */
        if (count <= 0) return;
        uint8_t mem[256];
        display_memory(m_regs.sp, count * 2, mem, true);
        fl_push_clip(x(), top(), w(), h() - HEADER_H);
        fl_font(DATA_FONT, DATA_SIZE);
        char text[16];
        for (int i = 0; i < count; i++) {
            const int ry = top() + 4 + i * lh;
            const unsigned addr = (m_regs.sp + i * 2) & 0xFFFF;
            const unsigned word = mem[i * 2] | (mem[i * 2 + 1] << 8);
            if (i == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x(), ry, w(), lh);
            }
            if (i == 0) {
                fl_color(COLOR_AMBER);
                fl_draw(">", x() + 6, ry + asc);
            }
            fl_color(COLOR_DIM);
            snprintf(text, sizeof(text), "+%02X", (i * 2) & 0xFF);
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
            case FL_PUSH: {
                const int r = row_at(Fl::event_y());
                if (r < 0 || !dbg_ready()) return 1;
                const hwaddr addr = (m_regs.sp + r * 2) & 0xFFFF;
                if (Fl::event_clicks() > 0) {
                    uint8_t word[2];
                    debugger_read_memory(fz_debugger(), addr, 2, word);
                    goto_disassembly(word[0] | (word[1] << 8));
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
    int row_at(int ey) const
    {
        if (ey < top() + 4) return -1;
        const int r = (ey - top() - 4) / data_line_height();
        return r < (h() - HEADER_H - 8) / data_line_height() ? r : -1;
    }

    regs_t m_regs = {};
    int m_hover = -1;
};

/* ------------------------------------------------------------------ */
/*  Breakpoints                                                        */
/* ------------------------------------------------------------------ */

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
            if ((int) i == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x(), ry, w(), lh);
                fl_color(COLOR_BREAK);
                fl_draw("x", x() + w() - cw * 2, ry + asc);
            }
            fl_color(COLOR_BREAK);
            fl_pie(x() + 8, ry + (lh - 9) / 2, 9, 9, 0, 360);
            hex16(text, sizeof(text), addrs[i]);
            fl_color(COLOR_CYAN);
            fl_draw(text, x() + 24, ry + asc);
            const char* sym = debugger_get_symbol(fz_debugger(), addrs[i]);
            if (sym) {
                fl_color(COLOR_AMBER);
                fl_draw(sym, x() + 24 + cw * 6, ry + asc);
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
            case FL_PUSH: {
                const int r = row_at(Fl::event_y());
                if (r < 0) return 1;
                if (Fl::event_x() >= x() + w() - char_width() * 3) {
                    debugger_clear_breakpoint(fz_debugger(), addrs[r]);
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
        m_input->tooltip("Address (hex) or symbol, then Enter");
        m_add = new Fl_Button(X, Y, 10, 10, "Add");
        style_button(m_add);
        m_add->callback(cb_add, this);
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
        char text[16];
        snprintf(text, sizeof(text), "%d", count > 0 ? count : 0);
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
        if (dbg_ready() && parse_address(self->m_input->value(), &addr)) {
            debugger_set_breakpoint(fz_debugger(), addr);
            self->m_input->value("");
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
            const bool is_bp = debugger_is_breakpoint_set(dbg, addr);
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
                fl_color(COLOR_BREAK);
                fl_pie(x() + 5, yy + (lh - 10) / 2, 10, 10, 0, 360);
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
        Fl_Menu_Item items[] = {
            { "Run to Cursor", 0, nullptr, nullptr, pause_only, 0, 0, 0, 0 },
            { "Set PC Here", 0, nullptr, nullptr, pause_only | FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Toggle Breakpoint", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
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
                goto_memory(addr);
                break;
            case 4:
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
        m_input->tooltip("Address (hex) or symbol, then Enter");
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
        if (parse_address(self->m_input->value(), &addr)) self->go(addr);
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

    MemView(int X, int Y, int W, int H, Fl_Scrollbar* bar)
        : Fl_Widget(X, Y, W, H), m_bar(bar)
    {
        m_prev.resize(0x10000);
        m_snapshot.resize(0x10000);
    }

    bool cp437 = false;
    int follow_addr = -1;

    void go(hwaddr addr, bool select = true)
    {
        addr &= 0xFFFF;
        if (select) {
            m_sel = (int) addr;
            m_nibble = -1;
        }
        const int row = (int) addr / COLS;
        const int rows = visible_rows();
        if (row < m_top || row >= m_top + rows - 1) m_top = row - (select ? 0 : rows / 3);
        clamp_top();
        sync_bar();
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

        /* Faint guides every 4 bytes */
        fl_color(COLOR_BG);
        for (int g = 4; g < COLS; g += 4) fl_rectf(m_hex_x + hex_offset(g) - m_cw + m_cw / 2, y(), 1, h());

        fl_font(DATA_FONT, DATA_SIZE);
        char buf[16];
        const bool paused = fz_paused();
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
                const bool sel = addr == m_sel;
                const bool hover = addr == m_hover;
                const bool followed = follow_addr >= 0 && (addr == follow_addr || addr == follow_addr + 1);
                const bool changed = m_have_prev && paused && b != m_prev[addr];
                if (sel || hover || followed) {
                    fl_color(sel ? COLOR_SELECT : followed ? COLOR_PC : COLOR_HOVER);
                    fl_rectf(m_hex_x + hex_offset(c) - 2, yy, m_cw * 2 + 4, lh);
                    fl_rectf(m_ascii_x + c * m_cw, yy, m_cw, lh);
                }
                if (sel && m_nibble >= 0) {
                    snprintf(buf, sizeof(buf), config.debugger.hex_upper ? "%X_" : "%x_", m_nibble);
                    fl_color(COLOR_AMBER);
                } else {
                    hex8(buf, sizeof(buf), b);
                    fl_color(changed ? COLOR_CHANGED : b == 0 ? COLOR_DIM : COLOR_TEXT);
                }
                fl_draw(buf, m_hex_x + hex_offset(c), yy + asc);

                char ch[8];
                if (cp437) {
                    const unsigned cp = s_cp437[b] ? s_cp437[b] : ' ';
                    const int n = fl_utf8encode(cp, ch);
                    ch[n] = '\0';
                } else {
                    ch[0] = isprint(b) ? (char) b : '.';
                    ch[1] = '\0';
                }
                fl_color(changed ? COLOR_CHANGED : (isprint(b) || cp437) ? COLOR_TEXT : COLOR_DIM);
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
                const int a = addr_at(Fl::event_x(), Fl::event_y());
                if (a >= 0) {
                    m_sel = a;
                    m_nibble = -1;
                    redraw();
                }
                take_focus();
                return 1;
            }
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
        if (m_sel < 0) return 0;
        int move = 0;
        switch (k) {
            case FL_Left:  move = -1; break;
            case FL_Right: move = 1; break;
            case FL_Up:    move = -COLS; break;
            case FL_Down:  move = COLS; break;
            case FL_Page_Up:   move = -COLS * (visible_rows() - 1); break;
            case FL_Page_Down: move = COLS * (visible_rows() - 1); break;
            case FL_Escape: m_nibble = -1; redraw(); return 1;
            default: break;
        }
        if (move) {
            int a = m_sel + move;
            if (a < 0) a = 0;
            if (a > 0xFFFF) a = 0xFFFF;
            go((hwaddr) a);
            return 1;
        }
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
    int m_sel = -1;
    int m_hover = -1;
    int m_nibble = -1;
    int m_cw = 8;
    int m_hex_x = 0;
    int m_ascii_x = 0;
    std::vector<uint8_t> m_prev;
    std::vector<uint8_t> m_snapshot;
    bool m_have_prev = false;
    bool m_have_snapshot = false;
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
        m_input->tooltip("Address (hex) or symbol, then Enter");
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
        m_mmu = new MmuStrip(X, Y, 10, 10);
        m_bar = new Fl_Scrollbar(X, Y, 10, 10);
        m_bar->type(FL_VERTICAL);
        m_bar->linesize(1);
        m_bar->color(COLOR_SURFACE);
        m_bar->callback(cb_bar, this);
        m_view = new MemView(X, Y, 10, 10, m_bar);
        resizable(m_view);
        end();
        resize(X, Y, W, H);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        const int by = Y + HEADER_H + 4;
        m_input->resize(X + 6, by, 130, BAR_H - 8);
        m_follow->resize(X + 142, by, 74, BAR_H - 8);
        m_cp437->resize(X + 224, by, 74, BAR_H - 8);
        const int my = Y + HEADER_H + BAR_H;
        const int mh = 26;
        m_mmu->resize(X, my, W, mh);
        m_bar->resize(X + W - 14, my + mh, 14, H - (my + mh - Y));
        m_view->resize(X, my + mh, W - 14, H - (my + mh - Y));
    }

    void refresh(bool paused, bool force) override
    {
        if (paused && force) m_view->snapshot();
        m_mmu->refresh();
        const int follow = m_follow->value();
        if (follow > 0) {
            const regs_t r = current_regs();
            const uint16_t values[] = { 0, r.pc, r.sp, r.hl, r.de, r.bc, r.ix, r.iy };
            m_view->follow_addr = values[follow];
            m_view->go(values[follow], false);
            char text[32];
            snprintf(text, sizeof(text), "FOLLOW %s", m_follow->text());
            extra(text);
        } else {
            m_view->follow_addr = -1;
            extra("");
        }
        m_view->redraw();
    }

    void go(hwaddr addr)
    {
        char text[8];
        hex16(text, sizeof(text), addr);
        m_input->value(text);
        m_follow->value(0);
        m_view->follow_addr = -1;
        extra("");
        m_view->go(addr);
    }

private:
    static void cb_go(Fl_Widget*, void* data)
    {
        MemoryPanel* self = static_cast<MemoryPanel*>(data);
        hwaddr addr;
        if (parse_address(self->m_input->value(), &addr)) self->go(addr);
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

    Fl_Input* m_input;
    Fl_Choice* m_follow;
    Fl_Check_Button* m_cp437;
    MmuStrip* m_mmu;
    Fl_Scrollbar* m_bar;
    MemView* m_view;
};

/* ------------------------------------------------------------------ */
/*  Debugger                                                           */
/* ------------------------------------------------------------------ */

constexpr int DEFAULT_LEFT_W = FZ_SCREEN_WIDTH;
constexpr int DEFAULT_SCREEN_H = FZ_SCREEN_HEIGHT;
constexpr int DEFAULT_RIGHT_W = 300;
constexpr int DEFAULT_BREAKPOINTS_H = 150;
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
    SplitPane* lower = nullptr;  /* stack / breakpoints */
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
        breakpoints->refresh(paused, force);
        disasm->refresh(paused, force);
        memory->refresh(paused, force);
        video_window_refresh(paused, force);
    }
};

static void request_refresh()
{
    if (s_impl) s_impl->refresh(true);
    if (main_window()) main_window()->update_status();
}

static void goto_memory(hwaddr addr)
{
    if (s_impl) s_impl->memory->go(addr);
}

static void goto_disassembly(hwaddr addr)
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
    config_save_hook = layout_save;

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
    m->breakpoints = new BreakpointsPanel(0, 0, DEFAULT_RIGHT_W, DEFAULT_BREAKPOINTS_H);
    m->breakpoints->end();
    dock->end();

    m->left->set(screen, m->memory, DEFAULT_SCREEN_H, 120, 140);
    m->lower->set(m->stack, m->breakpoints, DEFAULT_BREAKPOINTS_H, 60, 90);
    m->right->set(m->regs, m->lower, RegistersView::preferred_height(), 120, 150);
    m->rest->set(m->disasm, m->right, DEFAULT_RIGHT_W, 260, 240);
    m->root->set(m->left, m->rest, default_left_w(), MemoryPanel::preferred_width(), 520);

    Impl* impl = m;
    dock->set_root(m->root, [impl](bool expanded) {
        impl->root->collapse(!expanded);
        impl->left->collapse(!expanded);
        if (!expanded) video_window_suspend();
    });
}

Debugger::~Debugger()
{
    video_window_destroy();
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

void Debugger::default_body_size(int& w, int& h) const
{
    w = default_left_w() + DEFAULT_DISASM_W + DEFAULT_RIGHT_W + 2 * SplitPane::GAP;
    h = DEFAULT_SCREEN_H + DEFAULT_MEMORY_H + SplitPane::GAP;
}

void Debugger::apply_layout()
{
    const LayoutConfig& c = g_layout;
    m->root->fixed_size(c.left_w > 0 ? c.left_w : default_left_w());
    m->left->fixed_size(c.screen_h > 0 ? c.screen_h : DEFAULT_SCREEN_H);
    m->rest->fixed_size(c.right_w > 0 ? c.right_w : DEFAULT_RIGHT_W);
    m->lower->fixed_size(c.breakpoints_h > 0 ? c.breakpoints_h : DEFAULT_BREAKPOINTS_H);
    if (c.video_shown && m->dock->docks_visible()) {
        /* Open it once the main window is on screen, so it floats above it */
        Fl::add_timeout(0.3, [](void*) {
            if (main_window() && main_window()->shown() && main_window()->debug_layout()) {
                video_window_show(true);
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
    g_layout.breakpoints_h = m->lower->fixed_size();
    video_window_capture_config();
}

void Debugger::reset_layout()
{
    g_layout = LayoutConfig();
    video_window_show(false);
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
