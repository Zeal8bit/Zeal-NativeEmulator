/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Docked debugger: toolbar, CPU registers (right top), Disassembler and
 * Breakpoints tabs (right), Memory/VRAM/MMU/Semihost tabs (bottom). */

#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/fl_draw.H>
#include <FL/fl_utf8.h>
#include <FL/fl_ask.H>
#include "ui.h"

#if CONFIG_ENABLE_DEBUGGER
extern "C" {
#include "debugger/zeal_debugger.h"
}
#endif

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

/* Indices in config_panels (frontend/common/panels.c) */
enum {
    P_VIDEO = 0, P_BREAKPOINTS = 1, P_CPU = 2, P_MEMORY = 3,
    P_DISASSEMBLER = 4, P_VRAM = 5, P_MMU = 6, P_SEMIHOST = 7,
};

constexpr int BAR_H = 30;       /* input rows on top of panels */
constexpr int TAB_H = 26;       /* Fl_Tabs header */
constexpr int CPU_H = 228;      /* register panel height */

static Debugger::Impl* s_impl;

static void hex16(char* out, size_t size, unsigned value)
{
    snprintf(out, size, hex16_fmt(), value & 0xFFFF);
}

static void hex8(char* out, size_t size, unsigned value)
{
    snprintf(out, size, hex8_fmt(), value & 0xFF);
}

static void style_input(Fl_Input* in)
{
    in->textfont(MONO_FONT);
    in->textsize(MONO_SIZE);
    in->color(COLOR_BG);
    in->textcolor(COLOR_TEXT);
    in->cursor_color(COLOR_TEXT);
    in->box(FL_FLAT_BOX);
}

static void style_button(Fl_Button* b)
{
    b->labelsize(12);
    b->box(FL_FLAT_BOX);
    b->down_box(FL_FLAT_BOX);
    b->color(COLOR_HOVER);
    b->selection_color(COLOR_ACCENT);
    b->labelcolor(COLOR_TEXT);
    b->clear_visible_focus();
}

static void style_browser(Fl_Browser* b)
{
    b->textfont(MONO_FONT);
    b->textsize(MONO_SIZE);
    b->color(COLOR_BG);
    b->textcolor(COLOR_TEXT);
    b->selection_color(COLOR_PC);
    b->box(FL_FLAT_BOX);
    b->column_char('\t');
}

static void request_refresh();
static void goto_memory(hwaddr addr);
static void goto_disassembly(hwaddr addr);

/* ------------------------------------------------------------------ */
/*  Panel base                                                         */
/* ------------------------------------------------------------------ */

class Panel : public Fl_Group {
public:
    Panel(int X, int Y, int W, int H, const char* label = nullptr)
        : Fl_Group(X, Y, W, H, label)
    {
        box(FL_FLAT_BOX);
        color(COLOR_PANEL);
        labelsize(12);
        selection_color(COLOR_PANEL);
    }
    virtual void refresh(bool paused, bool force) = 0;
};

/* ------------------------------------------------------------------ */
/*  Toolbar                                                            */
/* ------------------------------------------------------------------ */

class Toolbar : public Fl_Group {
public:
    Toolbar(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H)
    {
        box(FL_FLAT_BOX);
        color(COLOR_PANEL);
        int bx = X + 8;
        const int by = Y + 4;
        const int bh = H - 8;
        auto make = [&](const char* label, int w, Fl_Callback* cb, const char* tip) {
            Fl_Button* b = new Fl_Button(bx, by, w, bh, label);
            style_button(b);
            b->callback(cb);
            b->tooltip(tip);
            bx += w + 6;
            return b;
        };
        m_run = make("Pause", 84, cb_run, "Pause or continue execution (Cmd/Ctrl+F5, F6)");
        make("Step Into", 84, cb_step, "Execute one instruction (Cmd/Ctrl+F11)");
        make("Step Over", 84, cb_step_over, "Step over calls (Cmd/Ctrl+F10)");
        bx += 12;
        make("Breakpoint", 84, cb_breakpoint, "Toggle a breakpoint at PC (Cmd/Ctrl+F9)");
        make("Reset", 70, cb_reset, "Reset the machine (Cmd/Ctrl+Shift+Backspace)");
        m_state = new Fl_Box(bx + 12, by, 260, bh);
        m_state->labelsize(12);
        m_state->labelfont(FL_HELVETICA_BOLD);
        m_state->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Box* spacer = new Fl_Box(X + W - 1, Y, 1, H);
        resizable(spacer);
        end();
    }

    void update(bool paused)
    {
        m_run->label(paused ? "Continue" : "Pause");
        if (paused) {
            regs_t regs;
            debugger_get_registers(fz_debugger(), &regs);
            char pc[8];
            hex16(pc, sizeof(pc), regs.pc);
            snprintf(m_text, sizeof(m_text), "PAUSED at $%s", pc);
            m_state->labelcolor(COLOR_LABEL);
        } else {
            snprintf(m_text, sizeof(m_text), "RUNNING");
            m_state->labelcolor(fl_rgb_color(0x6c, 0xc6, 0x6c));
        }
        m_state->label(m_text);
        redraw();
    }

private:
    static void cb_run(Fl_Widget*, void*)
    {
        dbg_t* dbg = fz_debugger();
        if (fz_paused()) debugger_continue(dbg); else debugger_pause(dbg);
        request_refresh();
    }
    static void cb_step(Fl_Widget*, void*)      { debugger_step(fz_debugger()); }
    static void cb_step_over(Fl_Widget*, void*) { debugger_step_over(fz_debugger()); }
    static void cb_breakpoint(Fl_Widget*, void*)
    {
        debugger_breakpoint(fz_debugger());
        request_refresh();
    }
    static void cb_reset(Fl_Widget*, void*)
    {
        fz_reset();
        request_refresh();
    }

    Fl_Button* m_run;
    Fl_Box* m_state;
    char m_text[48] = "";
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

class CpuPanel : public Panel {
public:
    CpuPanel(int X, int Y, int W, int H) : Panel(X, Y, W, H)
    {
        Fl_Box* title = new Fl_Box(X + 8, Y + 4, 200, 20, "REGISTERS");
        title->labelsize(11);
        title->labelfont(FL_HELVETICA_BOLD);
        title->labelcolor(COLOR_DIM);
        title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        const int row_h = 25;
        for (int id = 0; id < R_COUNT; id++) {
            const int col = id / 7;
            const int row = id % 7;
            const int fx = X + 8 + col * 140;
            const int fy = Y + 28 + row * row_h;
            Fl_Button* name = new Fl_Button(fx, fy, 40, row_h - 3, s_reg_names[id]);
            name->box(FL_FLAT_BOX);
            name->color(COLOR_PANEL);
            name->labelfont(MONO_FONT);
            name->labelsize(MONO_SIZE);
            name->labelcolor(COLOR_ACCENT);
            name->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            name->clear_visible_focus();
            name->callback(cb_goto, (void*) (intptr_t) id);
            name->tooltip(reg_is_8bit(id) ? nullptr : "Show this address in the memory viewer");
            Fl_Input* in = new Fl_Input(fx + 42, fy, 64, row_h - 3);
            style_input(in);
            in->maximum_size(reg_is_8bit(id) ? 2 : 4);
            in->when(FL_WHEN_ENTER_KEY_ALWAYS);
            in->callback(cb_edit, (void*) (intptr_t) id);
            in->tooltip("Edit while paused; press Enter to apply");
            m_inputs[id] = in;
        }

        static const char* flag_names[6] = { "S", "Z", "H", "P/V", "N", "C" };
        static const int flag_bits[6] = { 7, 6, 4, 2, 1, 0 };
        const int fy = Y + 28 + 7 * row_h + 6;
        for (int i = 0; i < 6; i++) {
            Fl_Check_Button* b = new Fl_Check_Button(X + 8 + i * 46, fy, 46, 22, flag_names[i]);
            b->labelsize(12);
            b->labelcolor(COLOR_TEXT);
            b->selection_color(COLOR_ACCENT);
            b->clear_visible_focus();
            b->callback(cb_flag, (void*) (intptr_t) flag_bits[i]);
            m_flags[i] = b;
            m_flag_bits[i] = flag_bits[i];
        }
        resizable(nullptr);
        end();
    }

    void refresh(bool paused, bool force) override
    {
        dbg_t* dbg = fz_debugger();
        regs_t regs;
        debugger_get_registers(dbg, &regs);
        char text[8];
        for (int id = 0; id < R_COUNT; id++) {
            const uint16_t value = reg_get(regs, id);
            Fl_Input* in = m_inputs[id];
            if (paused && force) {
                m_changed[id] = m_have_prev && value != m_prev[id];
                m_prev[id] = value;
            } else if (!paused) {
                m_changed[id] = false;
            }
            if (Fl::focus() != in) {
                if (reg_is_8bit(id)) hex8(text, sizeof(text), value);
                else hex16(text, sizeof(text), value);
                if (strcmp(in->value(), text) != 0) in->value(text);
            }
            in->textcolor(m_changed[id] ? COLOR_CHANGED : COLOR_TEXT);
            if (paused) in->activate(); else in->deactivate();
            m_values[id] = value;
        }
        if (paused && force) m_have_prev = true;
        for (int i = 0; i < 6; i++) {
            m_flags[i]->value((regs.f >> m_flag_bits[i]) & 1);
            if (paused) m_flags[i]->activate(); else m_flags[i]->deactivate();
        }
        redraw();
    }

    uint16_t value(int id) const { return m_values[id]; }

private:
    static void cb_goto(Fl_Widget* w, void* data)
    {
        const int id = (int) (intptr_t) data;
        if (reg_is_8bit(id)) return;
        CpuPanel* self = static_cast<CpuPanel*>(w->parent());
        goto_memory(self->m_values[id]);
    }

    static void cb_edit(Fl_Widget* w, void* data)
    {
        const int id = (int) (intptr_t) data;
        if (!fz_paused()) return;
        const char* text = static_cast<Fl_Input*>(w)->value();
        char* end = nullptr;
        const unsigned long v = strtoul(text[0] == '$' ? text + 1 : text, &end, 16);
        if (end == text || (end && *end != '\0')) {
            fl_beep();
            request_refresh();
            return;
        }
        dbg_t* dbg = fz_debugger();
        regs_t regs;
        debugger_get_registers(dbg, &regs);
        reg_set(regs, id, (uint16_t) v);
        debugger_set_registers(dbg, &regs);
        Fl::focus(nullptr);
        request_refresh();
    }

    static void cb_flag(Fl_Widget* w, void* data)
    {
        if (!fz_paused()) return;
        const int bit = (int) (intptr_t) data;
        dbg_t* dbg = fz_debugger();
        regs_t regs;
        debugger_get_registers(dbg, &regs);
        if (static_cast<Fl_Check_Button*>(w)->value()) regs.f |= (uint8_t) (1u << bit);
        else regs.f &= (uint8_t) ~(1u << bit);
        debugger_set_registers(dbg, &regs);
        request_refresh();
    }

    Fl_Input* m_inputs[R_COUNT] = {};
    Fl_Check_Button* m_flags[6] = {};
    int m_flag_bits[6] = {};
    uint16_t m_values[R_COUNT] = {};
    uint16_t m_prev[R_COUNT] = {};
    bool m_changed[R_COUNT] = {};
    bool m_have_prev = false;
};

/* ------------------------------------------------------------------ */
/*  Disassembler                                                       */
/* ------------------------------------------------------------------ */

class DisasmView : public Fl_Widget {
public:
    DisasmView(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H) {}

    void go(hwaddr addr)
    {
        m_base = addr & 0xFFFF;
        m_history.clear();
        redraw();
    }

    /** Keep PC visible without jumping on every step. */
    void follow(hwaddr pc)
    {
        for (size_t i = 0; i + 1 < m_rows.size(); i++) {
            if (m_rows[i].addr == pc) return;
        }
        go(pc);
    }

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(COLOR_BG);
        fl_rectf(x(), y(), w(), h());
        dbg_t* dbg = fz_debugger();
        if (dbg == nullptr) {
            fl_pop_clip();
            return;
        }
        regs_t regs;
        debugger_get_registers(dbg, &regs);
        const bool paused = fz_paused();

        fl_font(MONO_FONT, MONO_SIZE);
        const int lh = fl_height() + 3;
        const int cw = (int) fl_width("0");
        const int text_dy = lh - fl_descent() - 2;
        const int x_addr = x() + 22;
        const int x_bytes = x_addr + cw * 6;
        const int x_instr = x_bytes + cw * 12;

        m_rows.clear();
        hwaddr addr = m_base;
        int yy = y() + 2;
        char buf[96];
        while (yy + lh <= y() + h() && addr <= 0xFFFF) {
            dbg_instr_t instr;
            memset(&instr, 0, sizeof(instr));
            const int size = debugger_disassemble_address(dbg, addr, &instr);
            if (instr.label[0]) {
                fl_color(COLOR_LABEL);
                snprintf(buf, sizeof(buf), "%s:", instr.label);
                fl_draw(buf, x_addr, yy + text_dy);
                yy += lh;
                if (yy + lh > y() + h()) break;
            }

            const bool is_pc = paused && addr == regs.pc;
            const bool is_bp = debugger_is_breakpoint_set(dbg, addr);
            if (is_pc) {
                fl_color(COLOR_PC);
                fl_rectf(x(), yy, w(), lh);
            } else if ((int) addr == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x(), yy, w(), lh);
            }
            if (is_bp) {
                fl_color(COLOR_BREAK);
                fl_pie(x() + 5, yy + (lh - 11) / 2, 11, 11, 0, 360);
            }
            if (is_pc) {
                fl_color(COLOR_LABEL);
                const int cy = yy + lh / 2;
                fl_polygon(x() + 9, cy - 5, x() + 17, cy, x() + 9, cy + 5);
            }

            hex16(buf, sizeof(buf), addr);
            fl_color(is_pc ? COLOR_TEXT : COLOR_DIM);
            fl_draw(buf, x_addr, yy + text_dy);

            const int n = size > 0 ? (size > 4 ? 4 : size) : 1;
            int bx = 0;
            for (int i = 0; i < n; i++) {
                bx += snprintf(buf + bx, sizeof(buf) - bx, hex8_fmt(), instr.opcodes[i]);
                buf[bx++] = ' ';
                buf[bx] = '\0';
            }
            fl_draw(buf, x_bytes, yy + text_dy);

            fl_color(COLOR_TEXT);
            fl_draw(instr.instruction, x_instr, yy + text_dy);

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
                if (Fl::event_x() < x() + 20 || Fl::event_clicks() > 0) {
                    debugger_toggle_breakpoint(fz_debugger(), (hwaddr) addr);
                    Fl::event_clicks(0);
                    request_refresh();
                }
                return 1;
            }
            case FL_MOUSEWHEEL: {
                const int dy = Fl::event_dy();
                for (int i = 0; i < (dy > 0 ? dy : -dy) * 2; i++) {
                    if (dy > 0) {
                        if (m_rows.size() < 2) break;
                        m_history.push_back(m_base);
                        m_base = m_rows[1].addr;
                    } else if (!m_history.empty()) {
                        m_base = m_history.back();
                        m_history.pop_back();
                    } else if (m_base > 0) {
                        m_base--;
                    }
                    redraw();
                    /* Rows are recomputed on draw; keep scrolling coarse */
                    break;
                }
                return 1;
            }
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

private:
    struct Row { int y; int h; hwaddr addr; };

    int row_at(int ey) const
    {
        for (const Row& r : m_rows) {
            if (ey >= r.y && ey < r.y + r.h) return (int) r.addr;
        }
        return -1;
    }

    hwaddr m_base = 0;
    std::vector<hwaddr> m_history;
    std::vector<Row> m_rows;
    int m_hover = -1;
};

class DisasmPanel : public Panel {
public:
    DisasmPanel(int X, int Y, int W, int H, const char* label) : Panel(X, Y, W, H, label)
    {
        m_input = new Fl_Input(X + 6, Y + 4, 140, BAR_H - 8);
        style_input(m_input);
        m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
        m_input->callback(cb_go, this);
        m_input->tooltip("Address (hex) or symbol, then Enter");
        Fl_Button* go = new Fl_Button(X + 150, Y + 4, 40, BAR_H - 8, "Go");
        style_button(go);
        go->callback(cb_go, this);
        Fl_Button* pc = new Fl_Button(X + 194, Y + 4, 40, BAR_H - 8, "PC");
        style_button(pc);
        pc->callback(cb_pc, this);
        pc->tooltip("Show the current PC");
        m_follow = new Fl_Check_Button(X + 240, Y + 4, 100, BAR_H - 8, "Follow PC");
        m_follow->labelsize(12);
        m_follow->labelcolor(COLOR_TEXT);
        m_follow->selection_color(COLOR_ACCENT);
        m_follow->value(1);
        m_follow->clear_visible_focus();
        Fl_Box* hint = new Fl_Box(X + 340, Y + 4, W - 346, BAR_H - 8);
        m_view = new DisasmView(X, Y + BAR_H, W, H - BAR_H);
        m_view->tooltip("Click the gutter or double-click a line to toggle a breakpoint");
        resizable(m_view);
        (void) hint;
        end();
    }

    void refresh(bool paused, bool force) override
    {
        if (paused && force && m_follow->value()) {
            regs_t regs;
            debugger_get_registers(fz_debugger(), &regs);
            m_view->follow(regs.pc);
        }
        m_view->redraw();
    }

    void go(hwaddr addr) { m_view->go(addr); }

private:
    static void cb_go(Fl_Widget*, void* data)
    {
        DisasmPanel* self = static_cast<DisasmPanel*>(data);
        hwaddr addr;
        if (parse_address(self->m_input->value(), &addr)) {
            self->m_follow->value(0);
            self->m_view->go(addr);
        } else {
            fl_beep();
        }
    }

    static void cb_pc(Fl_Widget*, void* data)
    {
        DisasmPanel* self = static_cast<DisasmPanel*>(data);
        regs_t regs;
        debugger_get_registers(fz_debugger(), &regs);
        self->m_follow->value(1);
        self->m_view->go(regs.pc);
    }

    Fl_Input* m_input;
    Fl_Check_Button* m_follow;
    DisasmView* m_view;
};

/* ------------------------------------------------------------------ */
/*  Breakpoints                                                        */
/* ------------------------------------------------------------------ */

class BreakpointsPanel : public Panel {
public:
    BreakpointsPanel(int X, int Y, int W, int H, const char* label) : Panel(X, Y, W, H, label)
    {
        m_list = new Fl_Hold_Browser(X, Y, W, H - BAR_H);
        style_browser(m_list);
        static const int widths[] = { 70, 0 };
        m_list->column_widths(widths);
        m_list->callback(cb_list, this);
        m_list->tooltip("Double-click to show in the disassembler");
        const int by = Y + H - BAR_H + 4;
        m_input = new Fl_Input(X + 6, by, 140, BAR_H - 8);
        style_input(m_input);
        m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
        m_input->callback(cb_add, this);
        m_input->tooltip("Address (hex) or symbol, then Enter");
        Fl_Button* add = new Fl_Button(X + 150, by, 50, BAR_H - 8, "Add");
        style_button(add);
        add->callback(cb_add, this);
        Fl_Button* del = new Fl_Button(X + 204, by, 64, BAR_H - 8, "Remove");
        style_button(del);
        del->callback(cb_remove, this);
        Fl_Button* clr = new Fl_Button(X + 272, by, 60, BAR_H - 8, "Clear");
        style_button(clr);
        clr->callback(cb_clear, this);
        resizable(m_list);
        end();
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        hwaddr bps[DBG_MAX_POINTS];
        const int count = debugger_get_breakpoints(fz_debugger(), bps, DBG_MAX_POINTS);
        std::vector<hwaddr> now(bps, bps + (count > 0 ? count : 0));
        if (now == m_addrs && !m_dirty) return;
        m_addrs = now;
        m_dirty = false;
        const int selected = m_list->value();
        m_list->clear();
        char line[128];
        char addr[8];
        for (hwaddr a : m_addrs) {
            hex16(addr, sizeof(addr), a);
            const char* sym = debugger_get_symbol(fz_debugger(), a);
            snprintf(line, sizeof(line), "@.$%s\t@.%s", addr, sym ? sym : "");
            m_list->add(line);
        }
        if (selected > 0 && selected <= m_list->size()) m_list->value(selected);
    }

    void invalidate() { m_dirty = true; }

private:
    static void cb_list(Fl_Widget*, void* data)
    {
        BreakpointsPanel* self = static_cast<BreakpointsPanel*>(data);
        const int line = self->m_list->value();
        if (line > 0 && Fl::event_clicks() > 0) {
            Fl::event_clicks(0);
            goto_disassembly(self->m_addrs[line - 1]);
        }
    }

    static void cb_add(Fl_Widget*, void* data)
    {
        BreakpointsPanel* self = static_cast<BreakpointsPanel*>(data);
        hwaddr addr;
        if (parse_address(self->m_input->value(), &addr)) {
            debugger_set_breakpoint(fz_debugger(), addr);
            self->m_input->value("");
            request_refresh();
        } else {
            fl_beep();
        }
    }

    static void cb_remove(Fl_Widget*, void* data)
    {
        BreakpointsPanel* self = static_cast<BreakpointsPanel*>(data);
        const int line = self->m_list->value();
        if (line > 0 && line <= (int) self->m_addrs.size()) {
            debugger_clear_breakpoint(fz_debugger(), self->m_addrs[line - 1]);
            request_refresh();
        }
    }

    static void cb_clear(Fl_Widget*, void* data)
    {
        BreakpointsPanel* self = static_cast<BreakpointsPanel*>(data);
        for (hwaddr a : self->m_addrs) debugger_clear_breakpoint(fz_debugger(), a);
        request_refresh();
    }

    Fl_Hold_Browser* m_list;
    Fl_Input* m_input;
    std::vector<hwaddr> m_addrs;
    bool m_dirty = true;
};

/* ------------------------------------------------------------------ */
/*  Memory viewer                                                      */
/* ------------------------------------------------------------------ */

#include "cp437.inc"

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

    void go(hwaddr addr)
    {
        addr &= 0xFFFF;
        m_sel = (int) addr;
        m_nibble = -1;
        const int row = (int) addr / COLS;
        const int rows = visible_rows();
        if (row < m_top || row >= m_top + rows - 1) m_top = row;
        clamp_top();
        sync_bar();
        redraw();
    }

    /** Remember memory at a pause so the next pause highlights changes. */
    void snapshot()
    {
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

    int visible_rows() const
    {
        fl_font(MONO_FONT, MONO_SIZE);
        const int lh = fl_height() + 3;
        return (h() - 4) / lh;
    }

    void sync_bar()
    {
        const int rows = visible_rows();
        m_bar->value(m_top, rows, 0, ROWS_TOTAL);
    }

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(COLOR_BG);
        fl_rectf(x(), y(), w(), h());
        fl_font(MONO_FONT, MONO_SIZE);
        const int lh = fl_height() + 3;
        const int cw = (int) fl_width("0");
        const int text_dy = lh - fl_descent() - 2;
        const int rows = visible_rows();
        geometry(cw);

        uint8_t mem[COLS * 256];
        const int count_rows = rows < 256 ? rows : 256;
        const int start = m_top * COLS;
        int len = count_rows * COLS;
        if (start + len > 0x10000) len = 0x10000 - start;
        if (len > 0) debugger_read_memory(fz_debugger(), (hwaddr) start, len, mem);

        char buf[16];
        for (int r = 0; r < count_rows; r++) {
            const int row_addr = start + r * COLS;
            if (row_addr >= 0x10000) break;
            const int yy = y() + 2 + r * lh;
            hex16(buf, sizeof(buf), (unsigned) row_addr);
            fl_color(COLOR_ACCENT);
            fl_draw(buf, x() + 6, yy + text_dy);
            for (int c = 0; c < COLS; c++) {
                const int addr = row_addr + c;
                const uint8_t b = mem[r * COLS + c];
                const bool sel = addr == m_sel;
                const bool hover = addr == m_hover;
                const bool changed = m_have_prev && fz_paused() && b != m_prev[addr];
                if (sel || hover) {
                    fl_color(sel ? COLOR_PC : COLOR_HOVER);
                    fl_rectf(m_hex_x + hex_offset(c) - 2, yy, cw * 2 + 4, lh);
                    fl_rectf(m_ascii_x + c * cw, yy, cw, lh);
                }
                if (sel && m_nibble >= 0) {
                    snprintf(buf, sizeof(buf), config.debugger.hex_upper ? "%X_" : "%x_", m_nibble);
                    fl_color(COLOR_LABEL);
                } else {
                    hex8(buf, sizeof(buf), b);
                    fl_color(changed ? COLOR_CHANGED : b == 0 ? COLOR_DIM : COLOR_TEXT);
                }
                fl_draw(buf, m_hex_x + hex_offset(c), yy + text_dy);

                char ch[8];
                if (cp437) {
                    const unsigned cp = s_cp437[b] ? s_cp437[b] : '.';
                    const int n = fl_utf8encode(cp, ch);
                    ch[n] = '\0';
                } else {
                    ch[0] = isprint(b) ? (char) b : '.';
                    ch[1] = '\0';
                }
                fl_color(changed ? COLOR_CHANGED : isprint(b) || cp437 ? COLOR_TEXT : COLOR_DIM);
                fl_draw(ch, m_ascii_x + c * cw, yy + text_dy);
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
        m_ascii_x = m_hex_x + hex_offset(COLS) + cw * 2;
    }

    int addr_at(int ex, int ey)
    {
        fl_font(MONO_FONT, MONO_SIZE);
        const int lh = fl_height() + 3;
        geometry((int) fl_width("0"));
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
    MemoryPanel(int X, int Y, int W, int H, const char* label) : Panel(X, Y, W, H, label)
    {
        m_input = new Fl_Input(X + 6, Y + 4, 140, BAR_H - 8);
        style_input(m_input);
        m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
        m_input->callback(cb_go, this);
        m_input->tooltip("Address (hex) or symbol, then Enter");
        Fl_Button* go = new Fl_Button(X + 150, Y + 4, 40, BAR_H - 8, "Go");
        style_button(go);
        go->callback(cb_go, this);
        m_cp437 = new Fl_Check_Button(X + 198, Y + 4, 80, BAR_H - 8, "CP437");
        m_cp437->labelsize(12);
        m_cp437->labelcolor(COLOR_TEXT);
        m_cp437->selection_color(COLOR_ACCENT);
        m_cp437->clear_visible_focus();
        m_cp437->callback(cb_cp437, this);
        Fl_Box* hint = new Fl_Box(X + 284, Y + 4, W - 290, BAR_H - 8,
                                  "Click a byte, type hex to edit, arrows to move");
        hint->labelsize(11);
        hint->labelcolor(COLOR_DIM);
        hint->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

        m_bar = new Fl_Scrollbar(X + W - 16, Y + BAR_H, 16, H - BAR_H);
        m_bar->type(FL_VERTICAL);
        m_bar->linesize(1);
        m_bar->callback(cb_bar, this);
        m_view = new MemView(X, Y + BAR_H, W - 16, H - BAR_H, m_bar);
        resizable(m_view);
        end();
        m_view->sync_bar();
    }

    void refresh(bool paused, bool force) override
    {
        if (paused && force) m_view->snapshot();
        m_view->redraw();
    }

    void go(hwaddr addr)
    {
        char text[8];
        hex16(text, sizeof(text), addr);
        m_input->value(text);
        m_view->go(addr);
    }

private:
    static void cb_go(Fl_Widget*, void* data)
    {
        MemoryPanel* self = static_cast<MemoryPanel*>(data);
        hwaddr addr;
        if (parse_address(self->m_input->value(), &addr)) self->m_view->go(addr);
        else fl_beep();
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
    Fl_Check_Button* m_cp437;
    Fl_Scrollbar* m_bar;
    MemView* m_view;
};

/* ------------------------------------------------------------------ */
/*  MMU                                                                */
/* ------------------------------------------------------------------ */

class MmuPanel : public Panel {
public:
    MmuPanel(int X, int Y, int W, int H, const char* label) : Panel(X, Y, W, H, label)
    {
        m_list = new Fl_Hold_Browser(X, Y, W, H);
        style_browser(m_list);
        static const int widths[] = { 60, 110, 120, 0 };
        m_list->column_widths(widths);
        m_list->callback(cb_list, this);
        m_list->tooltip("Double-click a page to view it in the memory viewer");
        resizable(m_list);
        end();
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        dbg_mmu_t info;
        memset(&info, 0, sizeof(info));
        const int selected = m_list->value();
        m_list->clear();
        m_list->add("@B49@C7@bPage\t@B49@C7@bVirtual\t@B49@C7@bPhysical\t@B49@C7@bDevice");
        if (!debugger_custom(fz_debugger(), ZEAL_DBG_OP_GET_MMU, &info)) {
            m_list->add("@.Could not get MMU information");
            return;
        }
        char line[160], v[8], p[16];
        for (int i = 0; i < ZEAL_DBG_MMU_PAGES; i++) {
            const dbg_mmu_entry_t* e = &info.entries[i];
            hex16(v, sizeof(v), e->virt_addr);
            snprintf(p, sizeof(p), config.debugger.hex_upper ? "%06X" : "%06x", (unsigned) e->phys_addr);
            snprintf(line, sizeof(line), "@.%d\t@.$%s\t@.$%s\t@.%s", i, v, p, e->device ? e->device : "Unmapped");
            m_list->add(line);
            m_virt[i] = e->virt_addr;
        }
        if (selected > 1 && selected <= m_list->size()) m_list->value(selected);
    }

private:
    static void cb_list(Fl_Widget*, void* data)
    {
        MmuPanel* self = static_cast<MmuPanel*>(data);
        const int line = self->m_list->value();
        if (line >= 2 && line < 2 + ZEAL_DBG_MMU_PAGES && Fl::event_clicks() > 0) {
            Fl::event_clicks(0);
            goto_memory(self->m_virt[line - 2]);
        }
    }

    Fl_Hold_Browser* m_list;
    uint16_t m_virt[ZEAL_DBG_MMU_PAGES] = {};
};

/* ------------------------------------------------------------------ */
/*  VRAM viewer                                                        */
/* ------------------------------------------------------------------ */

class VramImage : public Fl_Widget {
public:
    VramImage(int X, int Y, int W, int H, Fl_Box* info) : Fl_Widget(X, Y, W, H), m_info(info) {}

    void update(dbg_vram_t view, int zoom)
    {
        m_view = view;
        m_zoom = zoom;
        m_valid = fz_video_mode() != FZ_VIDEO_BITMAP && fz_vram_render(view, &m_img);
        if (!m_valid) {
            size(300, 40);
            redraw();
            return;
        }
        const int W = m_img.width * zoom;
        const int H = m_img.height * zoom;
        m_scaled.resize((size_t) W * H * 3);
        for (int yy = 0; yy < H; yy++) {
            const uint8_t* src = m_img.pixels + (size_t) (yy / zoom) * m_img.width * 3;
            uint8_t* dst = &m_scaled[(size_t) yy * W * 3];
            for (int xx = 0; xx < W; xx++) {
                const uint8_t* p = src + (xx / zoom) * 3;
                *dst++ = p[0];
                *dst++ = p[1];
                *dst++ = p[2];
            }
        }
        if (w() != W || h() != H) {
            size(W, H);
            if (parent()) parent()->redraw();
        }
        redraw();
    }

    void draw() override
    {
        if (!m_valid) {
            fl_color(COLOR_DIM);
            fl_font(FL_HELVETICA, 12);
            fl_draw("Not available in bitmap mode", x() + 8, y() + 20);
            return;
        }
        fl_draw_image(m_scaled.data(), x(), y(), w(), h(), 3);
        if (m_hover_cell >= 0) draw_preview();
    }

    int handle(int event) override
    {
        switch (event) {
            case FL_ENTER:
                return 1;
            case FL_LEAVE:
                m_hover_cell = -1;
                m_info->copy_label("");
                redraw();
                return 1;
            case FL_MOVE:
                hover(Fl::event_x(), Fl::event_y());
                return 1;
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

private:
    void hover(int ex, int ey)
    {
        if (!m_valid) return;
        const int px = (ex - x()) / m_zoom;
        const int py = (ey - y()) / m_zoom;
        const int cw = m_img.cell_width + 1;
        const int ch = m_img.cell_height + 1;
        const int cx = px / cw;
        const int cy = py / ch;
        const int cell = cy * m_img.columns + cx;
        if (px < 0 || py < 0 || cx >= m_img.columns || (cy + 1) * ch > m_img.height) {
            m_hover_cell = -1;
        } else {
            m_hover_cell = cell;
            m_hover_x = cx;
            m_hover_y = cy;
            m_mouse_x = ex;
            m_mouse_y = ey;
        }
        char text[64] = "";
        if (m_hover_cell >= 0) {
            switch (m_view) {
                case DBG_TILEMAP_LAYER0:
                case DBG_TILEMAP_LAYER1:
                    snprintf(text, sizeof(text), "Cell (%d, %d)  index %d", cx, cy, cell);
                    break;
                case DBG_TILESET: snprintf(text, sizeof(text), "Tile %d", cell); break;
                case DBG_PALETTE: snprintf(text, sizeof(text), "Color %d", cell); break;
                case DBG_FONT: snprintf(text, sizeof(text), "Char %d (0x%02X)", cell, cell); break;
                default: break;
            }
        }
        m_info->copy_label(text);
        redraw();
    }

    void draw_preview()
    {
        const int cw = m_img.cell_width + 2;
        const int ch = m_img.cell_height + 2;
        const int scale = 6;
        int bx = m_mouse_x + 16;
        int by = m_mouse_y + 16;
        const int bw = cw * scale + 8;
        const int bh = ch * scale + 8;
        if (bx + bw > x() + w()) bx = m_mouse_x - bw - 8;
        if (by + bh > y() + h()) by = m_mouse_y - bh - 8;
        fl_color(COLOR_PANEL);
        fl_rectf(bx, by, bw, bh);
        const int sx0 = m_hover_x * (m_img.cell_width + 1);
        const int sy0 = m_hover_y * (m_img.cell_height + 1);
        for (int yy = 0; yy < ch; yy++) {
            for (int xx = 0; xx < cw; xx++) {
                const int sx = sx0 + xx;
                const int sy = sy0 + yy;
                if (sx >= m_img.width || sy >= m_img.height) continue;
                const uint8_t* p = m_img.pixels + ((size_t) sy * m_img.width + sx) * 3;
                fl_color(fl_rgb_color(p[0], p[1], p[2]));
                fl_rectf(bx + 4 + xx * scale, by + 4 + yy * scale, scale, scale);
            }
        }
    }

    Fl_Box* m_info;
    fz_image_t m_img = {};
    std::vector<uint8_t> m_scaled;
    dbg_vram_t m_view = DBG_TILEMAP_LAYER0;
    int m_zoom = 1;
    bool m_valid = false;
    int m_hover_cell = -1;
    int m_hover_x = 0;
    int m_hover_y = 0;
    int m_mouse_x = 0;
    int m_mouse_y = 0;
};

class VramPanel : public Panel {
public:
    VramPanel(int X, int Y, int W, int H, const char* label) : Panel(X, Y, W, H, label)
    {
        m_choice = new Fl_Choice(X + 6, Y + 4, 110, BAR_H - 8);
        m_choice->add("Layer 0|Layer 1|Tileset|Palette|Font");
        m_choice->value(0);
        m_choice->textsize(12);
        m_choice->callback(cb_change, this);
        m_choice->clear_visible_focus();
        m_zoom = new Fl_Choice(X + 122, Y + 4, 60, BAR_H - 8);
        m_zoom->add("1x|2x|3x");
        m_zoom->value(0);
        m_zoom->textsize(12);
        m_zoom->callback(cb_change, this);
        m_zoom->clear_visible_focus();
        m_info = new Fl_Box(X + 190, Y + 4, W - 196, BAR_H - 8);
        m_info->labelsize(12);
        m_info->labelfont(MONO_FONT);
        m_info->labelcolor(COLOR_LABEL);
        m_info->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        m_scroll = new Fl_Scroll(X, Y + BAR_H, W, H - BAR_H);
        m_scroll->box(FL_FLAT_BOX);
        m_scroll->color(COLOR_BG);
        m_image = new VramImage(X, Y + BAR_H, 10, 10, m_info);
        m_scroll->end();
        resizable(m_scroll);
        end();
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        m_image->update((dbg_vram_t) m_choice->value(), m_zoom->value() + 1);
        m_scroll->redraw();
    }

private:
    static void cb_change(Fl_Widget*, void* data)
    {
        VramPanel* self = static_cast<VramPanel*>(data);
        self->m_scroll->scroll_to(0, 0);
        self->refresh(fz_paused(), true);
    }

    Fl_Choice* m_choice;
    Fl_Choice* m_zoom;
    Fl_Box* m_info;
    Fl_Scroll* m_scroll;
    VramImage* m_image;
};

/* ------------------------------------------------------------------ */
/*  Semihost counters                                                  */
/* ------------------------------------------------------------------ */

class SemihostPanel : public Panel {
public:
    SemihostPanel(int X, int Y, int W, int H, const char* label) : Panel(X, Y, W, H, label)
    {
        m_list = new Fl_Hold_Browser(X, Y, W, H - 22);
        style_browser(m_list);
        static const int widths[] = { 50, 210, 170, 0 };
        m_list->column_widths(widths);
        m_list->callback(cb_list, this);
        m_list->tooltip("Click a counter to toggle break-on-update");
        Fl_Box* foot = new Fl_Box(X + 6, Y + H - 22, W - 12, 22,
                                  "* running   ! break on update (click a row to toggle)");
        foot->labelsize(11);
        foot->labelcolor(COLOR_DIM);
        foot->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        resizable(m_list);
        end();
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        const int selected = m_list->value();
        m_list->clear();
        m_list->add("@B49@C7@b#\t@B49@C7@bus (min, avg, max)\t@B49@C7@bms (min, avg, max)\t@B49@C7@btotal (us, ms)");
        char line[192];
        for (int i = 0; i < fz_semihost_counter_count(); i++) {
            fz_counter_t c;
            if (!fz_semihost_counter(i, &c)) continue;
            snprintf(line, sizeof(line), "%s%d%s%s\t@.%llu, %llu, %llu\t@.%llu, %llu, %llu\t@.%llu, %llu",
                     c.break_on_update ? "@C3@." : "@.", i, c.running ? "*" : "", c.break_on_update ? "!" : "",
                     (unsigned long long) c.min_us, (unsigned long long) c.avg_us, (unsigned long long) c.max_us,
                     (unsigned long long) c.min_us / 1000, (unsigned long long) c.avg_us / 1000,
                     (unsigned long long) c.max_us / 1000,
                     (unsigned long long) c.total_us, (unsigned long long) c.total_us / 1000);
            m_list->add(line);
        }
        if (selected > 1 && selected <= m_list->size()) m_list->value(selected);
    }

private:
    static void cb_list(Fl_Widget*, void* data)
    {
        SemihostPanel* self = static_cast<SemihostPanel*>(data);
        const int line = self->m_list->value();
        if (line >= 2 && Fl::event() == FL_RELEASE) {
            fz_semihost_toggle_break(line - 2);
            self->refresh(fz_paused(), true);
        }
    }

    Fl_Hold_Browser* m_list;
};

/* ------------------------------------------------------------------ */
/*  Docks                                                              */
/* ------------------------------------------------------------------ */

static Fl_Tabs* make_tabs(int X, int Y, int W, int H)
{
    Fl_Tabs* tabs = new Fl_Tabs(X, Y, W, H);
    tabs->box(FL_FLAT_BOX);
    tabs->color(COLOR_HOVER);
    tabs->selection_color(COLOR_PANEL);
    tabs->labelsize(12);
    return tabs;
}

static void style_tab(Fl_Group* g)
{
    g->labelsize(12);
    g->labelcolor(COLOR_TEXT);
    g->selection_color(COLOR_PANEL);
}

/** Right dock: CPU registers on top (fixed height) and tabs below. */
class RightDock : public Fl_Group {
public:
    RightDock(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H)
    {
        box(FL_FLAT_BOX);
        color(COLOR_BG);
    }

    void set(CpuPanel* cpu, Fl_Tabs* tabs)
    {
        m_cpu = cpu;
        m_tabs = tabs;
        layout();
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        layout();
    }

    void layout()
    {
        if (!m_cpu || !m_tabs) return;
        const bool cpu = !config_panels[P_CPU].hidden;
        const int ch = cpu ? CPU_H : 0;
        if (cpu) m_cpu->show(); else m_cpu->hide();
        m_cpu->resize(x(), y(), w(), CPU_H);
        m_tabs->resize(x(), y() + ch + (cpu ? 4 : 0), w(), h() - ch - (cpu ? 4 : 0));
        redraw();
    }

private:
    CpuPanel* m_cpu = nullptr;
    Fl_Tabs* m_tabs = nullptr;
};

struct TabEntry {
    int panel;
    Panel* widget;
};

struct Debugger::Impl {
    DockLayout* dock = nullptr;
    Toolbar* toolbar = nullptr;
    RightDock* right = nullptr;
    Fl_Tabs* right_tabs = nullptr;
    Fl_Tabs* bottom_tabs = nullptr;
    CpuPanel* cpu = nullptr;
    DisasmPanel* disasm = nullptr;
    BreakpointsPanel* breakpoints = nullptr;
    MemoryPanel* memory = nullptr;
    VramPanel* vram = nullptr;
    MmuPanel* mmu = nullptr;
    SemihostPanel* semihost = nullptr;
    std::vector<TabEntry> right_entries;
    std::vector<TabEntry> bottom_entries;
    double last_refresh = 0;

    void apply_visibility(Fl_Tabs* tabs, const std::vector<TabEntry>& entries)
    {
        Fl_Widget* current = tabs->value();
        for (const TabEntry& e : entries) {
            if (tabs->find(e.widget) < tabs->children()) tabs->remove(e.widget);
        }
        const int cx = tabs->x();
        const int cy = tabs->y() + TAB_H;
        const int cw = tabs->w();
        const int ch = tabs->h() - TAB_H;
        Fl_Widget* first = nullptr;
        bool keep = false;
        for (const TabEntry& e : entries) {
            if (config_panels[e.panel].hidden) continue;
            e.widget->resize(cx, cy, cw, ch);
            tabs->add(e.widget);
            if (!first) first = e.widget;
            if (e.widget == current) keep = true;
        }
        if (keep) tabs->value(current);
        else if (first) tabs->value(first);
        tabs->redraw();
    }

    void refresh_panel(Panel* p, bool paused, bool force)
    {
        if (p && p->visible_r()) p->refresh(paused, force);
    }

    void refresh(bool force)
    {
        /* Nothing to show until the machine is bound and the UI complete */
        if (fz_debugger() == nullptr || toolbar == nullptr) return;
        const bool paused = fz_paused();
        const double now = now_seconds();
        if (!force) {
            if (paused || now - last_refresh < 0.1) return;
        }
        last_refresh = now;
        toolbar->update(paused);
        refresh_panel(cpu, paused, force);
        breakpoints->refresh(paused, force);
        refresh_panel(disasm, paused, force);
        /* Memory snapshots must be taken at every pause, visible or not */
        if (paused && force) memory->refresh(paused, force);
        else refresh_panel(memory, paused, force);
        refresh_panel(mmu, paused, force);
        refresh_panel(semihost, paused, force);
        /* The VRAM rasteriser is costly: refresh at most ~4 Hz while running */
        static double s_vram_at;
        if (vram->visible_r() && (force || now - s_vram_at > 0.25)) {
            s_vram_at = now;
            vram->refresh(paused, force);
        }
    }
};

static void request_refresh()
{
    if (s_impl) s_impl->refresh(true);
    if (main_window()) main_window()->update_status();
}

static void select_tab(Fl_Tabs* tabs, Panel* panel, int index)
{
    if (config_panels[index].hidden) {
        config_panels[index].hidden = false;
        if (main_window() && main_window()->debugger()) main_window()->debugger()->apply_panel_visibility();
        if (main_window()) main_window()->update_menus();
    }
    tabs->value(panel);
}

static void goto_memory(hwaddr addr)
{
    if (!s_impl) return;
    select_tab(s_impl->bottom_tabs, s_impl->memory, P_MEMORY);
    s_impl->memory->go(addr);
}

static void goto_disassembly(hwaddr addr)
{
    if (!s_impl) return;
    select_tab(s_impl->right_tabs, s_impl->disasm, P_DISASSEMBLER);
    s_impl->disasm->go(addr);
}

static void cb_tabs(Fl_Widget*, void*)
{
    if (s_impl) s_impl->refresh(true);
}

Debugger::Debugger(DockLayout* dock) : m(new Impl)
{
    s_impl = m;
    m->dock = dock;
    Fl_Widget* screen = dock->child(0);
    const int W = 400;
    const int H = 600;

    dock->begin();
    m->right = new RightDock(0, 0, W, H);
    m->cpu = new CpuPanel(0, 0, W, CPU_H);
    m->right_tabs = make_tabs(0, CPU_H + 4, W, H - CPU_H - 4);
    m->disasm = new DisasmPanel(0, CPU_H + 4 + TAB_H, W, H - CPU_H - 4 - TAB_H, "Disassembler");
    style_tab(m->disasm);
    m->breakpoints = new BreakpointsPanel(0, CPU_H + 4 + TAB_H, W, H - CPU_H - 4 - TAB_H, "Breakpoints");
    style_tab(m->breakpoints);
    m->right_tabs->end();
    m->right_tabs->callback(cb_tabs);
    m->right_tabs->when(FL_WHEN_CHANGED);
    m->right->end();
    m->right->set(m->cpu, m->right_tabs);

    const int BW = 1000;
    const int BH = 300;
    m->bottom_tabs = make_tabs(0, 0, BW, BH);
    m->memory = new MemoryPanel(0, TAB_H, BW, BH - TAB_H, "Memory");
    style_tab(m->memory);
    m->vram = new VramPanel(0, TAB_H, BW, BH - TAB_H, "VRAM");
    style_tab(m->vram);
    m->mmu = new MmuPanel(0, TAB_H, BW, BH - TAB_H, "MMU");
    style_tab(m->mmu);
    m->semihost = new SemihostPanel(0, TAB_H, BW, BH - TAB_H, "Semihost");
    style_tab(m->semihost);
    m->bottom_tabs->end();
    m->bottom_tabs->callback(cb_tabs);
    m->bottom_tabs->when(FL_WHEN_CHANGED);
    dock->end();

    m->right_entries = { { P_DISASSEMBLER, m->disasm }, { P_BREAKPOINTS, m->breakpoints } };
    m->bottom_entries = { { P_MEMORY, m->memory }, { P_VRAM, m->vram },
                          { P_MMU, m->mmu }, { P_SEMIHOST, m->semihost } };

    dock->set_children(screen, m->right, m->bottom_tabs);
    apply_panel_visibility();
}

Debugger::~Debugger()
{
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

void Debugger::apply_panel_visibility()
{
    m->apply_visibility(m->right_tabs, m->right_entries);
    m->apply_visibility(m->bottom_tabs, m->bottom_entries);
    m->right->layout();
    m->refresh(true);
}

void Debugger::reset_layout()
{
    apply_panel_visibility();
    m->right_tabs->value(m->disasm);
    m->bottom_tabs->value(m->memory);
}

void Debugger::capture_layout()
{
    if (!m->dock->docks_visible()) return;
    int sw, sh;
    m->dock->screen_size_for(m->dock->w(), m->dock->h(), sw, sh);
    config_panels[P_VIDEO].width = sw;
    config_panels[P_VIDEO].height = sh;
}

#else /* !CONFIG_ENABLE_DEBUGGER */

struct Debugger::Impl {};
Debugger::Debugger(DockLayout*) : m(nullptr) {}
Debugger::~Debugger() { delete m; }
Fl_Group* Debugger::toolbar(int x, int y, int w, int h) { return new Fl_Group(x, y, w, h); }
void Debugger::refresh(bool) {}
void Debugger::goto_memory(hwaddr) {}
void Debugger::goto_disassembly(hwaddr) {}
void Debugger::apply_panel_visibility() {}
void Debugger::reset_layout() {}
void Debugger::capture_layout() {}

#endif

} // namespace zui
