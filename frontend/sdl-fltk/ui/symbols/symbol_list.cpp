/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "symbols/symbol_list.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/navigation.h"
#include "memtools/pattern.h"
#include "theme/theme.h"

namespace zui::symbols {

using namespace zui::common;
using namespace zui::debugger;
using namespace zui::theme;

constexpr int COLS_H = 22;   /* column header row */
constexpr int GUTTER = 22;   /* breakpoint marker column */

static const char* kind_label(uint8_t kind)
{
    switch (kind) {
        case SYM_KIND_CODE: return "CODE";
        case SYM_KIND_DATA: return "DATA";
        case SYM_KIND_CONST: return "CONST";
        default: return "OTHER";
    }
}

static Fl_Color kind_color(uint8_t kind)
{
    switch (kind) {
        case SYM_KIND_CODE: return COLOR_GREEN;
        case SYM_KIND_DATA: return COLOR_CYAN;
        case SYM_KIND_CONST: return COLOR_TEXT;
        default: return COLOR_DIM;
    }
}

/** Code opens in the disassembly, data in the memory editor. */
static bool is_data(uint8_t kind)
{
    return kind == SYM_KIND_DATA || kind == SYM_KIND_CONST;
}

static std::string to_lower(const char* s)
{
    std::string out(s ? s : "");
    for (char& c : out) c = (char) tolower((unsigned char) c);
    return out;
}

/** Address in hex, honouring the Hex Upper preference. */
static void format_addr(char* out, size_t size, hwaddr addr)
{
    if (addr <= 0xFFFF) {
        hex16(out, size, addr);
    } else {
        snprintf(out, size, config.debugger.hex_upper ? "%06X" : "%06x", (unsigned) addr);
    }
}

SymbolList::SymbolList(int X, int Y, int W, int H, Fl_Scrollbar* bar)
    : Fl_Widget(X, Y, W, H), m_bar(bar)
{}

bool SymbolList::sync()
{
    dbg_t* dbg = fz_debugger();
    const uint64_t generation = dbg ? dbg->symbol_generation : 0;
    if (generation == m_generation) {
        return false;
    }
    m_generation = generation;
    m_all.clear();
    for (const symbols_t* b = dbg ? &dbg->symbols : nullptr; b; b = b->next) {
        for (unsigned i = 0; i < b->count; i++) {
            const symbol_t& s = b->array[i];
            if (s.name == nullptr) {
                continue;
            }
            m_all.push_back({ s.name, to_lower(s.name), s.addr, s.kind, s.local });
        }
    }
    apply();
    return true;
}

void SymbolList::filter(const char* text)
{
    m_filter = to_lower(text);
    m_top = 0;
    apply();
}

void SymbolList::filter_kind(int kind)
{
    m_kind = kind;
    m_top = 0;
    apply();
}

void SymbolList::sort_by(Sort key)
{
    if (key == m_sort) {
        m_ascending = !m_ascending;
    } else {
        m_sort = key;
        m_ascending = true;
    }
    apply();
}

bool SymbolList::filtered() const
{ return !m_filter.empty() || m_kind != KIND_ALL; }

void SymbolList::scroll_to(int row)
{
    const int max_top = std::max(0, (int) m_rows.size() - visible_rows());
    m_top = std::max(0, std::min(row, max_top));
    sync_bar();
    redraw();
}

void SymbolList::sync_bar()
{
    const int vis = visible_rows();
    m_bar->value(m_top, vis, 0, std::max((int) m_rows.size(), vis));
}

void SymbolList::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    scroll_to(m_top);
}

void SymbolList::draw()
{
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    fl_push_clip(x(), y(), w(), h());
    draw_columns();

    const int lh = row_h();
    const int asc = text_baseline(lh);
    const int ry0 = y() + COLS_H;
    if (m_rows.empty()) {
        fl_font(FL_HELVETICA, 12);
        fl_color(COLOR_DIM);
        const char* msg = m_all.empty()
            ? "No symbols loaded.\nClick Open Map... to load your program's map file\n(z88dk or SDCC; or start with -m program.map)."
            : "No symbol matches the filter.\nPress Escape in the filter field to clear it, or show all kinds.";
        fl_draw(msg, x() + 12, ry0 + 10, w() - 24, 60, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
        fl_pop_clip();
        return;
    }

    dbg_t* dbg = fz_debugger();
    const int kx = kind_x();
    const int ax = addr_x();
    char text[16];
    fl_font(DATA_FONT, DATA_SIZE);
    const int end = std::min((int) m_rows.size(), m_top + visible_rows() + 1);
    for (int r = m_top; r < end; r++) {
        const Symbol& s = m_all[(size_t) m_rows[(size_t) r]];
        const int ry = ry0 + (r - m_top) * lh;
        if (r == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(x(), ry, w(), lh);
        }
        if (dbg && debugger_has_breakpoint(dbg, s.addr)) {
            const bool on = debugger_is_breakpoint_enabled(dbg, s.addr);
            fl_color(COLOR_BREAK);
            const int d = 9;
            const int bx = x() + (GUTTER - d) / 2, by = ry + (lh - d) / 2;
            if (on) {
                fl_pie(bx, by, d, d, 0, 360);
            } else {
                fl_arc(bx, by, d, d, 0, 360);
            }
        }
        fl_font(DATA_FONT, DATA_SIZE);
        /* File-local labels (name@label) are less prominent */
        fl_color(s.local ? COLOR_DIM : COLOR_AMBER);
        fl_push_clip(x() + GUTTER, ry, kx - x() - GUTTER - 8, lh);
        fl_draw(s.name.c_str(), x() + GUTTER, ry + asc);
        fl_pop_clip();
        fl_color(kind_color(s.kind));
        fl_draw(kind_label(s.kind), kx, ry + asc);
        fl_font(DATA_FONT, DATA_SIZE);
        format_addr(text, sizeof(text), s.addr);
        fl_color(COLOR_CYAN);
        fl_draw(text, ax, ry + asc);
    }
    fl_pop_clip();
}

int SymbolList::handle(int event)
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
            scroll_to(m_top + Fl::event_dy() * 3);
            set_hover(row_at(Fl::event_y()));
            return 1;
        case FL_PUSH: {
            const int ey = Fl::event_y();
            if (ey < y() + COLS_H) {
                if (Fl::event_button() == FL_LEFT_MOUSE) {
                    const int ex = Fl::event_x();
                    sort_by(ex >= addr_x() - 8 ? Sort::Address : ex >= kind_x() - 8 ? Sort::Kind : Sort::Name);
                    if (m_changed) {
                        m_changed();
                    }
                }
                return 1;
            }
            const int r = row_at(ey);
            if (r < 0) {
                return 1;
            }
            const Symbol& s = m_all[(size_t) m_rows[(size_t) r]];
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                context_menu(s);
            } else if (Fl::event_button() == FL_LEFT_MOUSE) {
                if (is_data(s.kind)) {
                    goto_memory(s.addr);
                } else {
                    goto_disassembly(s.addr);
                }
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

int SymbolList::row_h() const
{ return data_line_height(); }

int SymbolList::visible_rows() const
{ return std::max(1, (h() - COLS_H) / row_h()); }

int SymbolList::row_at(int ey) const
{
    if (ey < y() + COLS_H) {
        return -1;
    }
    const int r = m_top + (ey - y() - COLS_H) / row_h();
    return r < (int) m_rows.size() ? r : -1;
}

void SymbolList::set_hover(int r)
{
    if (r == m_hover) {
        return;
    }
    m_hover = r;
    redraw();
}

int SymbolList::kind_x() const
{
    const int cw = char_width();
    const int tail_w = cw * (KIND_CHARS + 6);
    int kx = x() + GUTTER + cw * (int) std::max<size_t>(m_name_chars, 8) + cw * 3;
    const int max_x = x() + w() - tail_w - 12;
    return std::max(x() + GUTTER + cw * 8, std::min(kx, max_x));
}

int SymbolList::addr_x() const
{ return kind_x() + char_width() * KIND_CHARS; }

void SymbolList::draw_columns()
{
    fl_color(COLOR_RAISED);
    fl_rectf(x(), y(), w(), COLS_H);
    fl_color(COLOR_BORDER);
    fl_xyline(x(), y() + COLS_H - 1, x() + w());
    /* Column positions measure the data font: get them first */
    const int kx = kind_x(), ax = addr_x();
    fl_font(FL_HELVETICA, 11);
    draw_column("NAME", x() + GUTTER, m_sort == Sort::Name);
    draw_column("KIND", kx, m_sort == Sort::Kind);
    draw_column("ADDRESS (HEX)", ax, m_sort == Sort::Address);
}

void SymbolList::draw_column(const char* label, int cx, bool active)
{
    const int ty = y() + (COLS_H + (int) fl_height()) / 2 - fl_descent();
    fl_color(active ? COLOR_TEXT : COLOR_DIM);
    fl_draw(label, cx, ty);
    if (!active) {
        return;
    }
    /* Sort direction arrow */
    const int ax = cx + (int) fl_width(label) + 6;
    const int ay = y() + COLS_H / 2;
    fl_color(COLOR_AMBER);
    if (m_ascending) {
        fl_polygon(ax, ay + 2, ax + 8, ay + 2, ax + 4, ay - 3);
    } else {
        fl_polygon(ax, ay - 2, ax + 8, ay - 2, ax + 4, ay + 3);
    }
}

void SymbolList::apply()
{
    m_rows.clear();
    m_name_chars = 0;
    for (size_t i = 0; i < m_all.size(); i++) {
        if (!m_filter.empty() && m_all[i].lower.find(m_filter) == std::string::npos) {
            continue;
        }
        if (m_kind != KIND_ALL && m_all[i].kind != m_kind) {
            continue;
        }
        m_rows.push_back((int) i);
        m_name_chars = std::max(m_name_chars, m_all[i].name.size());
    }
    const std::vector<Symbol>& all = m_all;
    const bool asc = m_ascending;
    if (m_sort == Sort::Kind) {
        std::stable_sort(m_rows.begin(), m_rows.end(), [&](int a, int b) {
            const int k1 = all[(size_t) a].kind, k2 = all[(size_t) b].kind;
            if (k1 != k2) {
                return asc ? k1 < k2 : k1 > k2;
            }
            return all[(size_t) a].addr < all[(size_t) b].addr;
        });
    } else if (m_sort == Sort::Name) {
        std::stable_sort(m_rows.begin(), m_rows.end(), [&](int a, int b) {
            const int c = all[(size_t) a].lower.compare(all[(size_t) b].lower);
            if (c != 0) {
                return asc ? c < 0 : c > 0;
            }
            return all[(size_t) a].addr < all[(size_t) b].addr;
        });
    } else {
        std::stable_sort(m_rows.begin(), m_rows.end(), [&](int a, int b) {
            const hwaddr x1 = all[(size_t) a].addr, x2 = all[(size_t) b].addr;
            if (x1 != x2) {
                return asc ? x1 < x2 : x1 > x2;
            }
            return all[(size_t) a].lower < all[(size_t) b].lower;
        });
    }
    m_hover = -1;
    scroll_to(m_top);
}

void SymbolList::context_menu(const Symbol& s)
{
    dbg_t* dbg = fz_debugger();
    Fl_Menu_Item items[] = {
        { "Show in Disassembly", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Show in Memory", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Toggle Breakpoint", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Copy Name", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Copy Address", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) {
        return;
    }
    char text[16];
    switch ((int) (picked - items)) {
        case 0: goto_disassembly(s.addr); break;
        case 1: goto_memory(s.addr); break;
        case 2:
            if (dbg) {
                debugger_toggle_breakpoint(dbg, s.addr);
            }
            request_refresh();
            break;
        case 3: Fl::copy(s.name.c_str(), (int) s.name.size(), 1); break;
        case 4:
            format_addr(text, sizeof(text), s.addr);
            Fl::copy(text, (int) strlen(text), 1);
            break;
        default: break;
    }
}

} // namespace zui::symbols
