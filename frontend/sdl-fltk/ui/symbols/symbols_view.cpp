/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "symbols/symbols_view.h"
#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "frontend/sdl-fltk/bridge/symbols.h"
#include "symbols/map_files.h"
#include "symbols/symbol_list.h"
#include "theme/style.h"
#include "theme/theme.h"
#include "tools/tool_registry.h"
#include "widgets/filter_input.h"
#include "widgets/panel.h"

namespace zui::symbols {

using namespace zui::debugger;
using namespace zui::theme;
using namespace zui::tools;
using namespace zui::widgets;

namespace {

constexpr int SCROLL_W = 14;

widgets::Panel* make_symbols(int x, int y, int w, int h)
{
    return new SymbolsView(x, y, w, h);
}

tools::ToolViewRegistrar s_reg(tools::TOOL_CODE, 10, "Symbols", make_symbols);

} // namespace

SymbolsView::SymbolsView(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "SYMBOLS")
{
    m_filter = new FilterInput(X, Y, 10, 10);
    style_input(m_filter);
    m_filter->when(FL_WHEN_CHANGED);
    m_filter->callback(cb_filter, this);
    m_filter->tooltip("Case-insensitive substring of the symbol name\nEscape: clear");
    m_kind = new Fl_Choice(X, Y, 10, 10);
    m_kind->add("All kinds|Code|Data|Const|Other");
    m_kind->value(0);
    m_kind->textsize(12);
    m_kind->clear_visible_focus();
    m_kind->tooltip("Code: functions and labels (from code sections)\n"
                    "Data: variables (data, bss)\n"
                    "Const: read-only data\n"
                    "Other: no section (equates, absolute values)");
    m_kind->callback(cb_kind, this);
    m_open = add_button("Open Map...", "Load a map file, replacing the current symbols", cb_open);
    m_add = add_button("Add...", "Load another map file next to the current symbols\n(e.g. the OS map plus your program's)", cb_add);
    m_reload = add_button("Reload", "Reload the map files (after rebuilding the program)", cb_reload);
    m_clear = add_button("Clear", "Remove every symbol", cb_clear);
    m_bar = new Fl_Scrollbar(X, Y, 10, 10);
    m_bar->type(FL_VERTICAL);
    m_bar->linesize(1);
    m_bar->color(COLOR_SURFACE);
    m_bar->callback(cb_bar, this);
    m_list = new SymbolList(X, Y, 10, 10, m_bar);
    m_list->tooltip("Click: code opens in the disassembly, data in memory\n"
                    "Right-click: more    Click a column title to sort");
    m_list->on_change([this] { update_extra(); });
    resizable(m_list);
    end();
    resize(X, Y, W, H);
}

void SymbolsView::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    const int by = Y + HEADER_H + 4;
    /* Buttons on the right, filter takes what is left */
    int bx = X + W - 8;
    for (Fl_Button* b : { m_clear, m_reload, m_add, m_open }) {
        fl_font(b->labelfont(), b->labelsize());
        const int bw = (int) fl_width(b->label()) + 20;
        bx -= bw;
        b->resize(bx, by, bw, BAR_H - 8);
        bx -= 4;
    }
    const int kind_w = 100;
    bx -= 8 + kind_w;
    m_kind->resize(bx, by, kind_w, BAR_H - 8);
    m_filter->resize(X + 62, by, std::max(60, std::min(320, bx - 12 - (X + 62))), BAR_H - 8);
    const int ly = Y + HEADER_H + BAR_H;
    m_list->resize(X, ly, W - SCROLL_W, H - (ly - Y));
    m_bar->resize(X + W - SCROLL_W, ly, SCROLL_W, H - (ly - Y));
}

void SymbolsView::refresh(bool paused, bool force)
{
    (void) paused;
    (void) force;
    if (!dbg_ready()) return;
    m_list->sync();
    update_extra();
    m_list->redraw();
}

void SymbolsView::draw()
{
    Panel::draw();
    fl_font(FL_HELVETICA, 11);
    fl_color(COLOR_DIM);
    fl_draw("FILTER", x() + 10, y() + HEADER_H + BAR_H / 2 + (int) fl_height() / 2 - fl_descent());
}

void SymbolsView::update_extra()
{
    char count[32];
    if (m_list->filtered()) snprintf(count, sizeof(count), "%zu of %zu", m_list->shown(), m_list->total());
    else snprintf(count, sizeof(count), "%zu symbol%s", m_list->total(), m_list->total() == 1 ? "" : "s");
    /* Which map files they came from */
    const int maps = fz_map_count();
    char text[64];
    /* Saved project maps that could not be loaded: Reload retries them */
    int missing = 0;
    for (int i = 0; i < maps; i++) missing += !fz_map_loaded(i);
    if (missing > 0) {
        snprintf(text, sizeof(text), "%s  %d map%s missing", count, missing, missing == 1 ? "" : "s");
    } else if (maps == 0) {
        snprintf(text, sizeof(text), "%s", count);
    } else if (maps == 1) {
        snprintf(text, sizeof(text), "%s  %s", count, base_name(fz_map_path(0)));
    } else {
        snprintf(text, sizeof(text), "%s  %d maps", count, maps);
    }
    extra(text);

    /* Full paths in the Reload tooltip */
    std::string tip = "Reload the map files (after rebuilding the program)";
    for (int i = 0; i < maps; i++) {
        tip += "\n";
        tip += fz_map_path(i);
        if (!fz_map_loaded(i)) tip += " (not loaded)";
    }
    if (tip != m_reload_tip) {
        m_reload_tip = tip;
        m_reload->tooltip(m_reload_tip.c_str());
    }
    if (maps > 0) m_reload->activate(); else m_reload->deactivate();
    if (m_list->total() > 0 || maps > 0) m_clear->activate(); else m_clear->deactivate();
}

const char* SymbolsView::base_name(const char* path)
{
    const char* name = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }
    return name;
}

Fl_Button* SymbolsView::add_button(const char* label, const char* tip, Fl_Callback* cb)
{
    Fl_Button* b = new Fl_Button(x(), y(), 10, 10, label);
    style_button(b);
    b->tooltip(tip);
    b->callback(cb, this);
    return b;
}

void SymbolsView::after_change()
{
    refresh(fz_paused(), true);
}

void SymbolsView::cb_open(Fl_Widget*, void* data)
{
    if (map_open_dialog(false)) static_cast<SymbolsView*>(data)->after_change();
}

void SymbolsView::cb_add(Fl_Widget*, void* data)
{
    if (map_open_dialog(true)) static_cast<SymbolsView*>(data)->after_change();
}

void SymbolsView::cb_reload(Fl_Widget*, void* data)
{
    map_reload();
    static_cast<SymbolsView*>(data)->after_change();
}

void SymbolsView::cb_clear(Fl_Widget*, void* data)
{
    map_clear();
    static_cast<SymbolsView*>(data)->after_change();
}

void SymbolsView::cb_filter(Fl_Widget*, void* data)
{
    SymbolsView* self = static_cast<SymbolsView*>(data);
    self->m_list->filter(self->m_filter->value());
    self->update_extra();
}

void SymbolsView::cb_kind(Fl_Widget*, void* data)
{
    SymbolsView* self = static_cast<SymbolsView*>(data);
    /* Menu order: All, Code, Data, Const, Other */
    static const int kinds[] = { SymbolList::KIND_ALL, SYM_KIND_CODE, SYM_KIND_DATA, SYM_KIND_CONST, SYM_KIND_UNKNOWN };
    const int v = self->m_kind->value();
    self->m_list->filter_kind(v >= 0 && v < 5 ? kinds[v] : SymbolList::KIND_ALL);
    self->update_extra();
}

void SymbolsView::cb_bar(Fl_Widget*, void* data)
{
    SymbolsView* self = static_cast<SymbolsView*>(data);
    self->m_list->scroll_to(self->m_bar->value());
}

} // namespace zui::symbols
