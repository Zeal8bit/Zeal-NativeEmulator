/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/watch_panel.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_ask.H>
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "debugger/watch_breaks.h"
#include "debugger/watch_list.h"
#include "debugger/watch_model.h"
#include "theme/style.h"
#include "theme/theme.h"
#include "widgets/panel.h"

namespace zui::debugger {

using namespace zui::theme;
using namespace zui::widgets;

WatchPanel::WatchPanel(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "WATCH")
{
    watches_load();
    m_list = new WatchList(X, Y, 10, 10);
    m_list->tooltip("Click: show in memory    Left edge: break on write / enable / disable\n"
                    "Right-click: break on read/write, change type, move, remove...");
    m_list->changed = [this] { update(); };
    m_input = new Fl_Input(X, Y, 10, 10);
    style_input(m_input);
    m_input->maximum_size(MAX_EXPR + 4);
    m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
    m_input->callback(cb_add, this);
    m_input->tooltip("C000, $C000, symbol, table+4 (hex; #4 = decimal), (HL), (IX+2), (ptr)\n"
                     "Optional ,N sets a string's length. Enter adds.");
    m_type = new Fl_Choice(X, Y, 10, 10);
    for (int i = 0; i < W_COUNT; i++) m_type->add(WATCH_TYPES[i].tag);
    m_type->value(W_U8);
    m_type->textsize(12);
    m_type->clear_visible_focus();
    m_type->tooltip("Display type of the new watch");
    m_add = new Fl_Button(X, Y, 10, 10, "Add");
    style_button(m_add);
    m_add->callback(cb_add, this);
    resizable(m_list);
    end();
    resize(X, Y, W, H);
    update_extra();
}

void WatchPanel::refresh(bool paused, bool force)
{
    if (!dbg_ready()) return;
    /* Breaks follow the rows (and symbols reloaded from a map) */
    watch_breaks_sync();
    const regs_t regs = current_regs();
    for (Watch& w : watches()) {
        evaluate(w, regs);
        if (paused && force) {
            w.changed = w.has_prev && w.valid &&
                        (w.n != w.prev_n || memcmp(w.cur, w.prev, (size_t) w.n) != 0);
            w.addr_changed = w.has_prev && w.valid && shown_addr(w) != w.prev_addr;
            w.bits_changed = w.changed ? (uint8_t) (w.cur[0] ^ w.prev[0]) : 0;
            take_snapshot(w);
        } else if (!paused) {
            w.changed = w.addr_changed = false;
        }
    }
    update_extra();
    m_list->redraw();
}

void WatchPanel::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    const int iy = Y + H - BAR_H + 4;
    m_list->resize(X, Y + HEADER_H, W, H - HEADER_H - BAR_H);
    m_input->resize(X + 6, iy, W - 6 - 72 - 50 - 4, BAR_H - 8);
    m_type->resize(X + W - 50 - 72, iy, 68, BAR_H - 8);
    m_add->resize(X + W - 50, iy, 44, BAR_H - 8);
}

void WatchPanel::update()
{
    if (dbg_ready()) {
        const regs_t regs = current_regs();
        for (Watch& w : watches()) {
            evaluate(w, regs);
            /* Entries without a reference value (new, retyped) take the
             * current one, so that they only light up on real changes. */
            if (!w.has_prev) take_snapshot(w);
        }
    }
    update_extra();
    m_list->redraw();
}

void WatchPanel::update_extra()
{
    char text[32];
    const int n = (int) watches().size();
    if (m_list && m_list->first() > 0) snprintf(text, sizeof(text), "%d  +%d", n, m_list->first());
    else snprintf(text, sizeof(text), "%d", n);
    extra(text);
}

void WatchPanel::cb_add(Fl_Widget*, void* data)
{
    WatchPanel* self = static_cast<WatchPanel*>(data);
    std::vector<Watch>& list = watches();
    /* Spaces are never significant; '|' and '"' would break the saved line */
    std::string expr;
    for (const char* p = self->m_input->value(); *p; p++) {
        if (!isspace((unsigned char) *p)) expr += *p;
    }
    Watch w;
    w.type = self->m_type->value();
    if (w.type < 0 || w.type >= W_COUNT) w.type = W_U8;
    const size_t comma = expr.rfind(',');
    if (comma != std::string::npos) {
        const std::string n = expr.substr(comma + 1);
        const int len = atoi(n.c_str());
        if (n.empty() || n.find_first_not_of("0123456789") != std::string::npos || len < 1 || len > MAX_STR) {
            fl_beep();
            return;
        }
        w.len = len;
        expr.resize(comma);
    }
    w.expr = expr;
    hwaddr addr;
    if (!dbg_ready() || expr.empty() || expr.size() > MAX_EXPR ||
        expr.find_first_of("|\"") != std::string::npos || (int) list.size() >= MAX_WATCHES ||
        !eval_expr(expr, current_regs(), &addr)) {
        fl_beep();
        return;
    }
    list.push_back(w);
    watch_breaks_sync();
    self->m_input->value("");
    self->m_list->show_last();
    self->update();
}

} // namespace zui::debugger
