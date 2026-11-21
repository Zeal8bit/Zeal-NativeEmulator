/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/watch_list.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "debugger/watch_breaks.h"
#include "debugger/watch_model.h"
#include "theme/theme.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;

WatchList::WatchList(int X, int Y, int W, int H)
    : Fl_Widget(X, Y, W, H)
{}

void WatchList::draw()
{
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    if (!dbg_ready()) return;
    std::vector<Watch>& list = watches();
    const int lh = data_line_height();
    const int cw = char_width();
    const int asc = text_baseline(lh);
    fl_push_clip(x(), y(), w(), h());
    if (list.empty()) {
        fl_color(COLOR_DIM);
        fl_font(FL_HELVETICA, 11);
        fl_draw("No watches. Add an address, a symbol, table+4 or (HL) below.", x() + 8, y() + 4,
                w() - 16, lh * 2, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
        fl_pop_clip();
        return;
    }

    /* Column widths (in characters) fit the widest value shown */
    char hex[MAX_STR + 8], dec[16];
    int hex_chars = 2, dec_chars = 3;
    for (const Watch& wt : list) {
        format(wt, hex, sizeof(hex), dec, sizeof(dec));
        if (wt.type != W_STR && (int) strlen(hex) > hex_chars) hex_chars = (int) strlen(hex);
        if ((int) strlen(dec) > dec_chars) dec_chars = (int) strlen(dec);
    }
    int dec_w = dec_chars * cw;
    for (const Watch& wt : list) {
        if (wt.type == W_BITS && bits_width() > dec_w) dec_w = bits_width();
    }
    const int right = x() + w() - 6;
    const int dec_x = right - dec_w;
    const int hex_x = dec_x - cw - hex_chars * cw;
    const int addr_x = hex_x - cw - 4 * cw;
    const int name_x = x() + GUTTER;
    const int name_r = addr_x - cw;

    /* Column titles */
    fl_font(FL_HELVETICA, 10);
    fl_color(COLOR_DIM);
    const int ty = y() + head_h() - 4;
    fl_draw("EXPR", name_x, ty);
    fl_draw("ADDR", addr_x, ty);
    fl_draw("HEX", hex_x, ty);
    fl_draw("DEC", right - (int) fl_width("DEC"), ty);
    fl_color(COLOR_BORDER);
    fl_xyline(x() + 4, y() + head_h() - 1, x() + w() - 4);

    clamp_first();
    const int rows = visible_rows();
    const int hit = watch_hit_index();
    char text[16];
    for (int i = 0; i < rows && m_first + i < (int) list.size(); i++) {
        const int index = m_first + i;
        const Watch& wt = list[(size_t) index];
        const int ry = rows_y() + i * lh;
        if (index == hit) {
            /* This row's break stopped the machine */
            fl_color(COLOR_BREAK_ROW);
            fl_rectf(x(), ry, w(), lh);
        } else if (index == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(x(), ry, w(), lh);
        }
        /* Break marker: filled = breaks, outline = break disabled, dim
         * outline = not armed (the expression does not resolve yet) */
        hwaddr blo, bhi;
        const bool armed = wt.brk && watch_break_target(wt, &blo, &bhi);
        if (wt.brk) {
            const int d = 8;
            const int cx = x() + GUTTER / 2, cy = ry + lh / 2;
            fl_color(armed ? COLOR_BREAK : COLOR_DIM);
            if (wt.brk_on && armed) {
                fl_polygon(cx, cy - d / 2, cx + d / 2, cy, cx, cy + d / 2, cx - d / 2, cy);
            } else {
                fl_loop(cx, cy - d / 2, cx + d / 2, cy, cx, cy + d / 2, cx - d / 2, cy);
            }
        }
        /* Type tag (and break kind), right-aligned in the name column */
        fl_font(FL_HELVETICA, 10);
        char tag[16];
        if (wt.type == W_STR) snprintf(tag, sizeof(tag), "str%d", wt.len);
        else snprintf(tag, sizeof(tag), "%s", WATCH_TYPES[wt.type].tag);
        const char* kind = wt.brk ? watch_type_short((watchpoint_type_t) wt.brk) : "";
        const int kind_w = wt.brk ? (int) fl_width(kind) + 4 : 0;
        const int tag_w = (int) fl_width(tag) + kind_w;
        fl_color(COLOR_DIM);
        fl_draw(tag, name_r - tag_w, ry + asc);
        if (wt.brk) {
            fl_color(wt.brk_on && armed ? COLOR_BREAK : COLOR_DIM);
            fl_draw(kind, name_r - kind_w + 4, ry + asc);
        }
        /* Expression */
        fl_font(DATA_FONT, DATA_SIZE);
        fl_push_clip(name_x, ry, name_r - tag_w - 4 - name_x, lh);
        fl_color(COLOR_AMBER);
        fl_draw(wt.expr.c_str(), name_x, ry + asc);
        fl_pop_clip();
        /* Address */
        if (wt.valid) {
            hex16(text, sizeof(text), shown_addr(wt));
            fl_color(wt.addr_changed ? COLOR_CHANGED : COLOR_CYAN);
        } else {
            snprintf(text, sizeof(text), "????");
            fl_color(COLOR_DIM);
        }
        fl_draw(text, addr_x, ry + asc);
        /* Value */
        format(wt, hex, sizeof(hex), dec, sizeof(dec));
        const Fl_Color vc = !wt.valid ? COLOR_DIM : wt.changed ? COLOR_CHANGED : COLOR_TEXT;
        if (wt.type == W_STR && wt.valid) {
            fl_push_clip(hex_x, ry, right - hex_x, lh);
            fl_color(vc);
            fl_draw(hex, hex_x, ry + asc);
            fl_pop_clip();
        } else {
            fl_color(vc);
            fl_draw(hex, hex_x, ry + asc);
            fl_draw(dec, right - (int) strlen(dec) * cw, ry + asc);
            if (wt.type == W_BITS && wt.valid) draw_bits(wt, right - bits_width(), ry + asc);
        }
    }
    fl_pop_clip();
}

int WatchList::handle(int event)
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
            m_first += Fl::event_dy();
            clamp_first();
            set_hover(row_at(Fl::event_y()));
            if (changed) changed(); /* header shows the scroll offset */
            redraw();
            return 1; /* never let the wheel fall through */
        case FL_PUSH: {
            if (!dbg_ready()) return 1;
            const int r = row_at(Fl::event_y());
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                context_menu(r);
            } else if (r >= 0 && Fl::event_x() < x() + GUTTER) {
                toggle_break(r);
            } else if (r >= 0 && watches()[(size_t) r].valid) {
                goto_memory(shown_addr(watches()[(size_t) r]));
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

void WatchList::show_last()
{
    m_first = (int) watches().size() - visible_rows();
    clamp_first();
}

int WatchList::bits_width()
{
    fl_font(FL_HELVETICA, BITS_FONT);
    const int wd = (int) fl_width("0") * 8 + 3;
    fl_font(DATA_FONT, DATA_SIZE);
    return wd;
}

void WatchList::draw_bits(const Watch& wt, int bx, int baseline)
{
    fl_font(FL_HELVETICA, BITS_FONT);
    const int dw = (int) fl_width("0");
    const uint8_t b = wt.cur[0];
    const uint8_t diff = wt.changed ? wt.bits_changed : 0;
    for (int i = 0; i < 8; i++) {
        const uint8_t mask = (uint8_t) (0x80 >> i);
        fl_color((diff & mask) ? COLOR_CHANGED : (b & mask) ? COLOR_GREEN : COLOR_DIM);
        fl_draw((b & mask) ? "1" : "0", bx + i * dw + (i >= 4 ? 3 : 0), baseline);
    }
    fl_font(DATA_FONT, DATA_SIZE);
}

int WatchList::rows_y() const
{ return y() + head_h() + 2; }

int WatchList::visible_rows() const
{
    const int r = (h() - head_h() - 2) / data_line_height();
    return r > 0 ? r : 0;
}

void WatchList::clamp_first()
{
    int max_first = (int) watches().size() - visible_rows();
    if (max_first < 0) max_first = 0;
    if (m_first > max_first) m_first = max_first;
    if (m_first < 0) m_first = 0;
}

int WatchList::row_at(int ey) const
{
    if (ey < rows_y()) return -1;
    const int i = (ey - rows_y()) / data_line_height();
    if (i >= visible_rows()) return -1;
    const int r = m_first + i;
    return r < (int) watches().size() ? r : -1;
}

void WatchList::set_hover(int r)
{
    if (r == m_hover) return;
    m_hover = r;
    redraw();
}

void WatchList::toggle_break(int row)
{
    Watch& w = watches()[(size_t) row];
    hwaddr lo, hi;
    const char* why = "";
    if (!watch_break_target(w, &lo, &hi, &why)) {
        notif_show("No break on %s: %s", w.expr.c_str(), why);
        return;
    }
    if (w.brk == 0) {
        w.brk = WATCHPOINT_WRITE;
        w.brk_on = true;
    } else {
        w.brk_on = !w.brk_on;
    }
    watch_breaks_sync();
    if (changed) changed();
}

void WatchList::context_menu(int row)
{
    std::vector<Watch>& list = watches();
    const bool has_row = row >= 0;
    Watch* wt = has_row ? &list[(size_t) row] : nullptr;
    const int row_only = has_row ? 0 : FL_MENU_INACTIVE;
    const int valid = (wt && wt->valid) ? 0 : FL_MENU_INACTIVE;
    const bool ptr = wt && is_pointer(*wt);
    const int any = list.empty() ? FL_MENU_INACTIVE : 0;
    auto radio = [&](int t) {
        return FL_MENU_RADIO | ((wt && wt->type == t) ? FL_MENU_VALUE : 0);
    };
    /* Break choices: only for watches whose address cannot move */
    hwaddr lo = 0, hi = 0;
    const char* why = "";
    const bool can_break = wt && watch_break_target(*wt, &lo, &hi, &why);
    const int brk_ok = can_break ? 0 : FL_MENU_INACTIVE;
    auto brk_radio = [&](int type) {
        return FL_MENU_RADIO | brk_ok | ((wt && wt->brk == type) ? FL_MENU_VALUE : 0);
    };
    char brk_title[96];
    if (!wt) snprintf(brk_title, sizeof(brk_title), "Break");
    else if (!can_break && wt->brk) snprintf(brk_title, sizeof(brk_title), "Break: not armed, %s", why);
    else if (!can_break) snprintf(brk_title, sizeof(brk_title), "Break: not available, %s", why);
    else {
        char range[24];
        watch_range_text(range, sizeof(range), lo, hi);
        snprintf(brk_title, sizeof(brk_title), "Break on %s%s", range, ptr ? " (the pointer)" : "");
    }
    const char* toggle_label = (wt && wt->brk && !wt->brk_on) ? "Enable Break" : "Disable Break";
    enum {
        M_TYPE = 0,
        M_TYPE_FIRST = 1,
        M_TYPE_END = M_TYPE_FIRST + W_COUNT,
        M_LEN = M_TYPE_END + 1,
        M_BRK_TITLE,
        M_BRK_READ,
        M_BRK_WRITE,
        M_BRK_ACCESS,
        M_BRK_NEVER,
        M_BRK_TOGGLE,
        M_MEM,
        M_DIS,
        M_TARGET,
        M_UP,
        M_DOWN,
        M_REMOVE,
        M_REMOVE_ALL,
    };
    Fl_Menu_Item items[] = {
        { "Change Type", 0, nullptr, nullptr, FL_SUBMENU | row_only, 0, 0, 0, 0 },
        { WATCH_TYPES[0].label, 0, nullptr, nullptr, radio(0), 0, 0, 0, 0 },
        { WATCH_TYPES[1].label, 0, nullptr, nullptr, radio(1), 0, 0, 0, 0 },
        { WATCH_TYPES[2].label, 0, nullptr, nullptr, radio(2), 0, 0, 0, 0 },
        { WATCH_TYPES[3].label, 0, nullptr, nullptr, radio(3), 0, 0, 0, 0 },
        { WATCH_TYPES[4].label, 0, nullptr, nullptr, radio(4), 0, 0, 0, 0 },
        { WATCH_TYPES[5].label, 0, nullptr, nullptr, radio(5), 0, 0, 0, 0 },
        { WATCH_TYPES[6].label, 0, nullptr, nullptr, radio(6), 0, 0, 0, 0 },
        { WATCH_TYPES[7].label, 0, nullptr, nullptr, radio(7), 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "String Length...", 0, nullptr, nullptr,
          (wt && wt->type == W_STR) ? FL_MENU_DIVIDER : (FL_MENU_INACTIVE | FL_MENU_DIVIDER), 0, 0, 0, 0 },
        { brk_title, 0, nullptr, nullptr, FL_MENU_INACTIVE, 0, 0, 0, 0 },
        { "    on Read", 0, nullptr, nullptr, brk_radio(WATCHPOINT_READ), 0, 0, 0, 0 },
        { "    on Write", 0, nullptr, nullptr, brk_radio(WATCHPOINT_WRITE), 0, 0, 0, 0 },
        { "    on Access (read or write)", 0, nullptr, nullptr, brk_radio(WATCHPOINT_RW), 0, 0, 0, 0 },
        /* Always available on a row with a break, armed or not */
        { "    Never", 0, nullptr, nullptr,
          brk_radio(0) & ~((wt && wt->brk) ? FL_MENU_INACTIVE : 0), 0, 0, 0, 0 },
        { toggle_label, 0, nullptr, nullptr,
          ((wt && wt->brk && can_break) ? 0 : FL_MENU_INACTIVE) | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Show in Memory", 0, nullptr, nullptr, valid, 0, 0, 0, 0 },
        { "Show in Disassembly", 0, nullptr, nullptr, valid, 0, 0, 0, 0 },
        { "Show Pointer in Memory", 0, nullptr, nullptr,
          (ptr && wt->valid ? 0 : FL_MENU_INACTIVE) | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Move Up", 0, nullptr, nullptr, row > 0 ? 0 : FL_MENU_INACTIVE, 0, 0, 0, 0 },
        { "Move Down", 0, nullptr, nullptr,
          has_row && row + 1 < (int) list.size() ? 0 : FL_MENU_INACTIVE, 0, 0, 0, 0 },
        { "Remove", 0, nullptr, nullptr, row_only | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Remove All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    static_assert(sizeof(items) / sizeof(items[0]) == M_REMOVE_ALL + 2, "menu layout");
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) return;
    const int id = (int) (picked - items);
    if (id >= M_TYPE_FIRST && id < M_TYPE_END) {
        list[(size_t) row].type = id - M_TYPE_FIRST;
        list[(size_t) row].has_prev = false;
    } else {
        switch (id) {
            case M_LEN: {
                char def[8];
                snprintf(def, sizeof(def), "%d", list[(size_t) row].len);
                const char* answer = fl_input("String length in characters (1-64, decimal):", def);
                if (answer == nullptr) return;
                const int len = atoi(answer);
                if (len < 1 || len > MAX_STR) {
                    fl_beep();
                    return;
                }
                list[(size_t) row].len = len;
                list[(size_t) row].has_prev = false;
                break;
            }
            case M_BRK_READ: wt->brk = WATCHPOINT_READ; wt->brk_on = true; break;
            case M_BRK_WRITE: wt->brk = WATCHPOINT_WRITE; wt->brk_on = true; break;
            case M_BRK_ACCESS: wt->brk = WATCHPOINT_RW; wt->brk_on = true; break;
            case M_BRK_NEVER: wt->brk = 0; break;
            case M_BRK_TOGGLE: wt->brk_on = !wt->brk_on; break;
            case M_MEM: goto_memory(shown_addr(list[(size_t) row])); return;
            case M_DIS: goto_disassembly(shown_addr(list[(size_t) row])); return;
            case M_TARGET: goto_memory(list[(size_t) row].addr); return;
            case M_UP: std::swap(list[(size_t) row], list[(size_t) row - 1]); break;
            case M_DOWN: std::swap(list[(size_t) row], list[(size_t) row + 1]); break;
            case M_REMOVE: list.erase(list.begin() + row); break;
            case M_REMOVE_ALL: list.clear(); break;
            default: return;
        }
    }
    /* Type, length and removal all change what the breaks cover */
    watch_breaks_sync();
    m_hover = -1;
    clamp_first();
    if (changed) changed();
}

} // namespace zui::debugger
