/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "profiler/hot_code_view.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <FL/Fl_Button.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "frontend/sdl-fltk/bridge/profile.h"
#include "profiler/hot_list.h"
#include "theme/style.h"
#include "theme/theme.h"
#include "tools/tool_registry.h"
#include "widgets/icon_button.h"
#include "widgets/mode_button.h"
#include "widgets/panel.h"

namespace zui::profiler {

using namespace zui::common;
using namespace zui::debugger;
using namespace zui::theme;
using namespace zui::tools;
using namespace zui::widgets;

namespace {

constexpr int TOP_ADDRESSES = 512;  /* rows in Addresses mode */

widgets::Panel* make_hotcode(int x, int y, int w, int h)
{
    return new HotCodeView(x, y, w, h);
}

tools::ToolViewRegistrar s_reg(tools::TOOL_CODE, 20, "Hot Functions", make_hotcode);

} // namespace

HotCodeView::HotCodeView(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "HOT FUNCTIONS")
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

void HotCodeView::resize(int X, int Y, int W, int H)
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

void HotCodeView::refresh(bool paused, bool force)
{
    (void) paused;
    update_controls();
    const uint64_t total = fz_profile_total();
    /* Called ~4 Hz while visible: re-aggregate only when something changed */
    if (force || total != m_last_total || symbols_changed()) rebuild();
    update_status();
}

void HotCodeView::draw()
{
    Panel::draw();
    /* Status, right side of the toolbar row */
    fl_font(FL_HELVETICA, 12);
    fl_color(fz_profile_running() ? COLOR_GREEN : COLOR_DIM);
    const int sx = m_addrs->x() + m_addrs->w() + 12;
    fl_draw(m_status, sx, y() + HEADER_H, x() + w() - 10 - sx, BAR_H,
            FL_ALIGN_RIGHT | FL_ALIGN_CLIP);
}

void HotCodeView::cb_start(Fl_Widget*, void* data)
{
    HotCodeView* self = static_cast<HotCodeView*>(data);
    if (fz_profile_running()) fz_profile_stop();
    else fz_profile_start();
    self->update_controls();
    self->update_status();
    self->rebuild();
}

void HotCodeView::cb_reset(Fl_Widget*, void* data)
{
    HotCodeView* self = static_cast<HotCodeView*>(data);
    fz_profile_reset();
    self->rebuild();
    self->update_status();
}

void HotCodeView::cb_mode(Fl_Widget*, void* data)
{
    HotCodeView* self = static_cast<HotCodeView*>(data);
    self->m_list->scroll_to_row(0);
    self->rebuild();
}

void HotCodeView::cb_bar(Fl_Widget*, void* data)
{
    HotCodeView* self = static_cast<HotCodeView*>(data);
    self->m_list->scroll_to_row(self->m_bar->value());
}

void HotCodeView::update_controls()
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

void HotCodeView::update_status()
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

bool HotCodeView::symbols_changed()
{
    const uintptr_t sig = symbols_signature();
    if (sig == m_sym_sig) return false;
    return true;
}

uintptr_t HotCodeView::symbols_signature()
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

void HotCodeView::load_symbols()
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

int HotCodeView::symbol_index(hwaddr addr) const
{
    auto it = std::upper_bound(m_syms.begin(), m_syms.end(), addr,
                               [](hwaddr a, const Sym& s) { return a < s.addr; });
    if (it == m_syms.begin()) return -1;
    return (int) (it - m_syms.begin()) - 1;
}

void HotCodeView::rebuild()
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

void HotCodeView::set_empty_text()
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

} // namespace zui::profiler
