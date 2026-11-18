/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/memory_panel.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_ask.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/address_lookup.h"
#include "debugger/machine_state.h"
#include "debugger/mem_find_input.h"
#include "debugger/mem_view.h"
#include "debugger/mmu_strip.h"
#include "memory_tools.h"
#include "theme/style.h"
#include "theme/theme.h"
#include "widgets/panel.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::memtools;
using namespace zui::theme;
using namespace zui::widgets;

int MemoryPanel::preferred_width()
{
    const int cw = char_width();
    return 6 + cw * 6 + (16 * 3 + 1) * cw + cw + 16 * cw + 8 + 14;
}

MemoryPanel::MemoryPanel(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "MEMORY")
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

MemoryPanel::~MemoryPanel()
{ memtools::change_search_destroy(); }

void MemoryPanel::resize(int X, int Y, int W, int H)
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

void MemoryPanel::refresh(bool paused, bool force)
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

void MemoryPanel::go(hwaddr addr)
{
    set_input(addr);
    unfollow();
    m_view->go(addr);
}

void MemoryPanel::set_input(hwaddr addr)
{
    char text[8];
    hex16(text, sizeof(text), addr);
    m_input->value(text);
}

void MemoryPanel::unfollow()
{
    m_follow->value(0);
    m_view->follow_addr = -1;
    update_extra();
}

void MemoryPanel::update_extra()
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

void MemoryPanel::show_find(bool show)
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

void MemoryPanel::find_status(const char* text, Fl_Color c)
{
    m_find_status->copy_label(text);
    m_find_status->labelcolor(c);
    m_find_status->redraw();
}

void MemoryPanel::find(bool forward)
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

void MemoryPanel::show_changes()
{
    memtools::SnapshotSource src;
    MemView* view = m_view;
    src.get = [view](const uint8_t** prev, const uint8_t** cur) { return view->snapshots(prev, cur); };
    src.go = [this](unsigned addr) { go((hwaddr) addr); };
    memtools::change_search_show(src);
}

void MemoryPanel::cb_go(Fl_Widget*, void* data)
{
    MemoryPanel* self = static_cast<MemoryPanel*>(data);
    hwaddr addr;
    if (lookup_address(self->m_input, self->m_input->value(), &addr, SYM_KIND_DATA)) self->go(addr);
    else fl_beep();
}

void MemoryPanel::cb_follow(Fl_Widget*, void* data)
{
    static_cast<MemoryPanel*>(data)->refresh(fz_paused(), false);
}

void MemoryPanel::cb_cp437(Fl_Widget*, void* data)
{
    MemoryPanel* self = static_cast<MemoryPanel*>(data);
    self->m_view->cp437 = self->m_cp437->value() != 0;
    self->m_view->redraw();
}

void MemoryPanel::cb_bar(Fl_Widget*, void* data)
{
    MemoryPanel* self = static_cast<MemoryPanel*>(data);
    self->m_view->scroll_to_row(self->m_bar->value());
}

void MemoryPanel::cb_find_toggle(Fl_Widget*, void* data)
{
    MemoryPanel* self = static_cast<MemoryPanel*>(data);
    self->show_find(!self->m_find_shown);
}

void MemoryPanel::cb_find_changed(Fl_Widget*, void* data)
{
    MemoryPanel* self = static_cast<MemoryPanel*>(data);
    memtools::Pattern p;
    if (!memtools::parse_pattern(self->m_find->value(), p, nullptr)) p = memtools::Pattern();
    self->m_view->highlight(p);
    self->find_status(self->m_find->size() ? "Enter: next  Shift+Enter: previous" : "", COLOR_DIM);
}

void MemoryPanel::cb_find_next(Fl_Widget*, void* data)
{ static_cast<MemoryPanel*>(data)->find(true); }

void MemoryPanel::cb_find_prev(Fl_Widget*, void* data)
{ static_cast<MemoryPanel*>(data)->find(false); }

void MemoryPanel::cb_changes(Fl_Widget*, void* data)
{ static_cast<MemoryPanel*>(data)->show_changes(); }

} // namespace zui::debugger
