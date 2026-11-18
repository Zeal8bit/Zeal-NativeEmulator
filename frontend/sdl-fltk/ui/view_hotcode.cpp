/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Hot Functions (Code & Profiler window): statistical PC profiler.
 *
 * The machine layer (bridge_profile.c) samples the PC at random points of
 * emulated time into a 64K histogram; this view aggregates it per function
 * (nearest preceding debugger symbol, or 256-byte buckets without symbols)
 * or lists the hottest individual addresses with their disassembly. */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include "tool_window.h"

extern "C" {
#include "frontend/sdl-fltk/bridge_profile.h"
}

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

namespace {

constexpr int TOP_ADDRESSES = 512;  /* rows in Addresses mode */

struct HotRow {
    hwaddr addr;        /* function start / bucket start / PC */
    hwaddr end;         /* last address covered (inclusive) */
    std::string name;   /* symbol, symbol+offset or bucket range */
    bool symbol;        /* name is a real symbol (amber) */
    uint64_t samples;
};

struct Sym {
    hwaddr addr;
    const char* name;
};

/** "12,345" */
void with_commas(char* out, size_t size, uint64_t v)
{
    char raw[32];
    snprintf(raw, sizeof(raw), "%llu", (unsigned long long) v);
    const int len = (int) strlen(raw);
    size_t o = 0;
    for (int i = 0; i < len && o + 1 < size; i++) {
        if (i > 0 && (len - i) % 3 == 0 && o + 2 < size) out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = '\0';
}

/** "$1234" honouring hex_upper */
void addr_text(char* out, size_t size, hwaddr addr)
{
    char h[8];
    hex16(h, sizeof(h), addr);
    snprintf(out, size, "$%s", h);
}

/* Segmented mode switch, styled like the tool window tabs */
class ModeButton : public Fl_Button {
public:
    ModeButton(int X, int Y, int W, int H, const char* label) : Fl_Button(X, Y, W, H, label)
    {
        type(FL_RADIO_BUTTON);
        box(FL_FLAT_BOX);
        clear_visible_focus();
    }

    void draw() override
    {
        const bool on = value() != 0;
        const bool hover = Fl::belowmouse() == this;
        fl_color(on ? COLOR_HOVER : hover ? COLOR_RAISED : COLOR_BG);
        fl_rectf(x(), y(), w(), h());
        fl_color(COLOR_BORDER);
        fl_rect(x(), y(), w(), h());
        if (on) {
            fl_color(COLOR_AMBER);
            fl_rectf(x() + 1, y() + h() - 3, w() - 2, 2);
        }
        fl_font(FL_HELVETICA, 12);
        fl_color(on ? COLOR_TEXT : COLOR_DIM);
        fl_draw(label(), x(), y(), w(), h(), FL_ALIGN_CENTER);
    }

    int handle(int event) override
    {
        if (event == FL_ENTER || event == FL_LEAVE) {
            redraw();
            return 1;
        }
        return Fl_Button::handle(event);
    }
};

/* The table: custom-drawn rows with hover, selection and a scrollbar */
class HotList : public Fl_Widget {
public:
    HotList(int X, int Y, int W, int H, Fl_Scrollbar* bar) : Fl_Widget(X, Y, W, H), m_bar(bar) {}

    std::vector<HotRow> rows;
    uint64_t total = 0;
    bool addresses = false;   /* Addresses mode: show disassembly column */
    const char* empty_title = "";
    const char* empty_hint = "";

    int line_h() const { return data_line_height() + 4; }
    int head_h() const { return 22; }
    int visible_rows() const { return std::max(1, (h() - head_h()) / line_h()); }

    void clamp_top()
    {
        const int max_top = std::max(0, (int) rows.size() - visible_rows());
        m_top = std::max(0, std::min(m_top, max_top));
    }

    void sync_bar()
    {
        clamp_top();
        m_bar->value(m_top, visible_rows(), 0, std::max((int) rows.size(), visible_rows()));
    }

    void scroll_to_row(int row)
    {
        m_top = row;
        clamp_top();
        redraw();
    }

    void draw() override
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

    int handle(int event) override
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
                if (idx < 0) return 1;
                const HotRow row = rows[(size_t) idx];
                m_selected = (int) row.addr;
                redraw();
                if (Fl::event_button() == FL_RIGHT_MOUSE) context_menu(row);
                else goto_disassembly(row.addr);
                return 1;
            }
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        sync_bar();
    }

private:
    void layout_columns(int cw)
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

    void draw_empty()
    {
        fl_font(FL_HELVETICA, 13);
        fl_color(COLOR_TEXT);
        fl_draw(empty_title, x() + 16, y() + 16, w() - 32, 22, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_CLIP);
        fl_font(FL_HELVETICA, 11);
        fl_color(COLOR_DIM);
        fl_draw(empty_hint, x() + 16, y() + 42, w() - 32, h() - 50,
                FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP | FL_ALIGN_CLIP);
    }

    void set_hover(int idx)
    {
        if (idx == m_hover) return;
        m_hover = idx;
        redraw();
    }

    int row_at(int ey) const
    {
        const int ry0 = y() + head_h();
        if (ey < ry0) return -1;
        const int idx = m_top + (ey - ry0) / line_h();
        return idx < (int) rows.size() ? idx : -1;
    }

    void context_menu(const HotRow& row)
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
        if (picked == nullptr) return;
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

    Fl_Scrollbar* m_bar;
    int m_top = 0;
    int m_hover = -1;
    int m_selected = -1;
    int m_x_name = 0, m_x_addr = 0, m_x_instr = 0, m_x_samples_r = 0, m_x_bar = 0;
};

} // namespace

class HotCodeView : public Panel {
public:
    HotCodeView(int X, int Y, int W, int H) : Panel(X, Y, W, H, "HOT FUNCTIONS")
    {
        m_start = new IconButton(X, Y, 10, 10, Icon::Play, "Start");
        m_start->callback(cb_start, this);
        m_start->tooltip("Start/stop sampling the program counter while the machine runs");
        m_reset = new Fl_Button(X, Y, 10, 10, "Reset");
        style_button(m_reset);
        m_reset->callback(cb_reset, this);
        m_reset->tooltip("Discard all samples");
        m_funcs = new ModeButton(X, Y, 10, 10, "Functions");
        m_funcs->callback(cb_mode, this);
        m_funcs->tooltip("Samples per function (nearest preceding symbol)");
        m_addrs = new ModeButton(X, Y, 10, 10, "Addresses");
        m_addrs->callback(cb_mode, this);
        m_addrs->tooltip("Hottest individual instructions");
        m_funcs->setonly();
        m_bar = new Fl_Scrollbar(X, Y, 10, 10);
        m_bar->type(FL_VERTICAL);
        m_bar->linesize(1);
        m_bar->color(COLOR_SURFACE);
        m_bar->callback(cb_bar, this);
        m_list = new HotList(X, Y, 10, 10, m_bar);
        m_list->tooltip("Click: show in disassembly    Right-click: more");
        resizable(m_list);
        end();
        resize(X, Y, W, H);
        update_controls();
        rebuild();
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        const int by = Y + HEADER_H + 4;
        const int bh = BAR_H - 8;
        m_start->resize(X + 6, by, 78, bh);
        m_reset->resize(X + 90, by, 60, bh);
        m_funcs->resize(X + 162, by, 84, bh);
        m_addrs->resize(X + 245, by, 84, bh);
        const int ly = Y + HEADER_H + BAR_H;
        m_bar->resize(X + W - 14, ly, 14, H - (ly - Y));
        m_list->resize(X, ly, W - 14, H - (ly - Y));
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        update_controls();
        const uint64_t total = fz_profile_total();
        /* Called ~4 Hz while visible: re-aggregate only when something changed */
        if (force || total != m_last_total || symbols_changed()) rebuild();
        update_status();
    }

    void draw() override
    {
        Panel::draw();
        /* Status, right side of the toolbar row */
        fl_font(FL_HELVETICA, 12);
        fl_color(fz_profile_running() ? COLOR_GREEN : COLOR_DIM);
        const int sx = m_addrs->x() + m_addrs->w() + 12;
        fl_draw(m_status, sx, y() + HEADER_H, x() + w() - 10 - sx, BAR_H,
                FL_ALIGN_RIGHT | FL_ALIGN_CLIP);
    }

private:
    static void cb_start(Fl_Widget*, void* data)
    {
        HotCodeView* self = static_cast<HotCodeView*>(data);
        if (fz_profile_running()) fz_profile_stop();
        else fz_profile_start();
        self->update_controls();
        self->update_status();
        self->rebuild();
    }

    static void cb_reset(Fl_Widget*, void* data)
    {
        HotCodeView* self = static_cast<HotCodeView*>(data);
        fz_profile_reset();
        self->rebuild();
        self->update_status();
    }

    static void cb_mode(Fl_Widget*, void* data)
    {
        HotCodeView* self = static_cast<HotCodeView*>(data);
        self->m_list->scroll_to_row(0);
        self->rebuild();
    }

    static void cb_bar(Fl_Widget*, void* data)
    {
        HotCodeView* self = static_cast<HotCodeView*>(data);
        self->m_list->scroll_to_row(self->m_bar->value());
    }

    void update_controls()
    {
        const bool running = fz_profile_running();
        if ((int) running == m_shown_running) return;
        m_shown_running = (int) running;
        m_start->icon(running ? Icon::Pause : Icon::Play);
        m_start->text(running ? "Stop" : "Start");
        m_start->icon_color(running ? COLOR_BREAK : COLOR_GREEN);
        extra(running ? "RECORDING" : "STOPPED");
        redraw();
    }

    void update_status()
    {
        char count[32];
        with_commas(count, sizeof(count), fz_profile_total());
        const double secs = (double) fz_profile_elapsed_us() / 1e6;
        char text[sizeof(m_status)];
        snprintf(text, sizeof(text), "%s samples \xC2\xB7 %.1f s", count, secs);
        if (strcmp(text, m_status) != 0) {
            snprintf(m_status, sizeof(m_status), "%s", text);
            damage(FL_DAMAGE_ALL, x(), y() + HEADER_H, w(), BAR_H);
        }
    }

    /** Cheap change detection of the debugger's symbol list (map loaded/cleared). */
    bool symbols_changed()
    {
        const uintptr_t sig = symbols_signature();
        if (sig == m_sym_sig) return false;
        return true;
    }

    static uintptr_t symbols_signature()
    {
        dbg_t* dbg = fz_debugger();
        if (dbg == nullptr) return 0;
        uintptr_t sig = 0;
        for (const symbols_t* s = &dbg->symbols; s != nullptr; s = s->next) {
            sig = sig * 31 + s->count;
            if (s->count > 0) sig ^= (uintptr_t) s->array[0].name;
        }
        return sig;
    }

    void load_symbols()
    {
        m_sym_sig = symbols_signature();
        m_syms.clear();
        dbg_t* dbg = fz_debugger();
        if (dbg == nullptr) return;
        for (const symbols_t* s = &dbg->symbols; s != nullptr; s = s->next) {
            for (unsigned i = 0; i < s->count && i < DBG_SYM_COUNT; i++) {
                const symbol_t& sym = s->array[i];
                if (sym.name == nullptr || sym.addr > 0xFFFF) continue;
                m_syms.push_back({ sym.addr, sym.name });
            }
        }
        std::stable_sort(m_syms.begin(), m_syms.end(),
                         [](const Sym& a, const Sym& b) { return a.addr < b.addr; });
        /* One name per address: keep the first one loaded */
        m_syms.erase(std::unique(m_syms.begin(), m_syms.end(),
                                 [](const Sym& a, const Sym& b) { return a.addr == b.addr; }),
                     m_syms.end());
    }

    /** Index of the nearest symbol at or before `addr`, or -1. */
    int symbol_index(hwaddr addr) const
    {
        auto it = std::upper_bound(m_syms.begin(), m_syms.end(), addr,
                                   [](hwaddr a, const Sym& s) { return a < s.addr; });
        if (it == m_syms.begin()) return -1;
        return (int) (it - m_syms.begin()) - 1;
    }

    void rebuild()
    {
        if (symbols_changed()) load_symbols();
        const uint32_t* counts = nullptr;
        const uint64_t total = fz_profile_histogram(&counts);
        m_last_total = total;
        std::vector<HotRow>& rows = m_list->rows;
        rows.clear();
        m_list->total = total;
        m_list->addresses = m_addrs->value() != 0;
        if (counts == nullptr || total == 0) {
            set_empty_text();
            m_list->sync_bar();
            m_list->redraw();
            return;
        }

        char name[96];
        char a[8];
        char b[8];
        if (!m_list->addresses) {
            /* Aggregate by function (or 256-byte bucket before the first symbol
             * and when no symbols are loaded) */
            std::vector<uint64_t> per(m_syms.size() + 256, 0);
            for (unsigned pc = 0; pc < FZ_PROFILE_SLOTS; pc++) {
                if (counts[pc] == 0) continue;
                const int si = symbol_index(pc);
                if (si >= 0) per[(size_t) si] += counts[pc];
                else per[m_syms.size() + (pc >> 8)] += counts[pc];
            }
            for (size_t i = 0; i < per.size(); i++) {
                if (per[i] == 0) continue;
                HotRow r;
                r.samples = per[i];
                if (i < m_syms.size()) {
                    r.addr = m_syms[i].addr;
                    r.end = i + 1 < m_syms.size() ? m_syms[i + 1].addr - 1 : 0xFFFF;
                    r.name = m_syms[i].name;
                    r.symbol = true;
                } else {
                    r.addr = (hwaddr) ((i - m_syms.size()) << 8);
                    r.end = r.addr + 0xFF;
                    hex16(a, sizeof(a), r.addr);
                    hex16(b, sizeof(b), r.end);
                    snprintf(name, sizeof(name), "$%s-$%s", a, b);
                    r.name = name;
                    r.symbol = false;
                }
                rows.push_back(std::move(r));
            }
        } else {
            for (unsigned pc = 0; pc < FZ_PROFILE_SLOTS; pc++) {
                if (counts[pc] == 0) continue;
                HotRow r;
                r.addr = pc;
                r.end = pc;
                r.samples = counts[pc];
                r.symbol = false;
                rows.push_back(std::move(r));
            }
        }
        std::sort(rows.begin(), rows.end(), [](const HotRow& x, const HotRow& y) {
            return x.samples != y.samples ? x.samples > y.samples : x.addr < y.addr;
        });
        if (m_list->addresses) {
            if (rows.size() > (size_t) TOP_ADDRESSES) rows.resize(TOP_ADDRESSES);
            /* Label only the rows kept: symbol+offset */
            for (HotRow& r : rows) {
                const int si = symbol_index(r.addr);
                if (si < 0) continue;
                const hwaddr off = r.addr - m_syms[(size_t) si].addr;
                if (off == 0) {
                    snprintf(name, sizeof(name), "%s", m_syms[(size_t) si].name);
                } else {
                    if (off < 0x100) hex8(a, sizeof(a), off);
                    else hex16(a, sizeof(a), off);
                    snprintf(name, sizeof(name), "%s+$%s", m_syms[(size_t) si].name, a);
                }
                r.name = name;
                r.symbol = true;
            }
        }
        m_list->sync_bar();
        m_list->redraw();
    }

    void set_empty_text()
    {
        if (!dbg_ready()) {
            m_list->empty_title = "Profiler unavailable";
            m_list->empty_hint = "No machine is running.";
        } else if (fz_profile_running()) {
            m_list->empty_title = "Recording... waiting for samples";
            m_list->empty_hint = "Samples are only taken while the machine executes: "
                                 "resume it and run the code you want to measure.";
        } else {
            m_list->empty_title = "Press Start, then run the code you want to measure";
            m_list->empty_hint = "The profiler samples the program counter at random points of "
                                 "emulated time (about every 0.5 ms) and shows where the CPU spends "
                                 "its time, per function or per instruction. Load a map file (-m) "
                                 "for function names; without one, samples are grouped in 256-byte "
                                 "address ranges. Click a row to show it in the disassembly.";
        }
    }

    IconButton* m_start;
    Fl_Button* m_reset;
    ModeButton* m_funcs;
    ModeButton* m_addrs;
    Fl_Scrollbar* m_bar;
    HotList* m_list;
    std::vector<Sym> m_syms;
    uintptr_t m_sym_sig = 1;    /* force the first load */
    uint64_t m_last_total = UINT64_MAX;
    int m_shown_running = -1;   /* force the first update */
    char m_status[64] = "";
};

static Panel* make_hotcode(int x, int y, int w, int h)
{
    return new HotCodeView(x, y, w, h);
}

static ToolViewRegistrar s_reg(TOOL_CODE, 20, "Hot Functions", make_hotcode);

#endif

} // namespace zui
