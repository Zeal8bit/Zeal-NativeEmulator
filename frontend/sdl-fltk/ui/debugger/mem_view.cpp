/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/mem_view.h"
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
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include <FL/fl_utf8.h>
#include "common/cp437.h"
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "debugger/watch_breaks.h"
#include "debugger/watch_model.h"
#include "theme/theme.h"
#include "memtools/memory_actions.h"
#include "memtools/pattern.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::memtools;
using namespace zui::theme;

MemView::MemView(int X, int Y, int W, int H, Fl_Scrollbar* bar)
    : Fl_Widget(X, Y, W, H), m_bar(bar)
{
    m_prev.resize(0x10000);
    m_snapshot.resize(0x10000);
}

int MemView::sel_lo() const
{ return m_anchor < m_sel ? m_anchor : m_sel; }

int MemView::sel_len() const
{ return m_sel < 0 ? 0 : sel_hi() - sel_lo() + 1; }

int MemView::top_addr() const
{ return m_top * COLS; }

void MemView::go(hwaddr addr, bool select, bool extend)
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

void MemView::select_range(int lo, int hi)
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

void MemView::highlight(const memtools::Pattern& p)
{
    m_find = p;
    redraw();
}

void MemView::snapshot()
{
    if (!dbg_ready()) return;
    m_prev.swap(m_snapshot);
    debugger_read_memory(fz_debugger(), 0, 0x10000, m_snapshot.data());
    m_have_prev = m_have_snapshot;
    m_have_snapshot = true;
}

bool MemView::snapshots(const uint8_t** prev, const uint8_t** cur) const
{
    if (!m_have_snapshot) return false;
    *prev = m_have_prev ? m_prev.data() : nullptr;
    *cur = m_snapshot.data();
    return true;
}

void MemView::write(unsigned addr, const std::vector<uint8_t>& data)
{
    if (!dbg_ready() || data.empty() || addr + data.size() > 0x10000) return;
    std::vector<uint8_t> copy(data);
    debugger_write_memory(fz_debugger(), (hwaddr) addr, (int) copy.size(), copy.data());
    memcpy(&m_snapshot[addr], copy.data(), copy.size());
    request_refresh();
}

void MemView::scroll_to_row(int row)
{
    m_top = row;
    clamp_top();
    redraw();
}

int MemView::visible_rows() const
{ return (h() - 4) / data_line_height(); }

void MemView::sync_bar()
{ m_bar->value(m_top, visible_rows(), 0, ROWS_TOTAL); }

void MemView::draw()
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
            const int watched = debugger_watchpoint_type_at(fz_debugger(), (hwaddr) addr);
            if (watched) {
                /* Underline watched bytes: red when writes stop, amber for reads only */
                fl_color((watched & WATCHPOINT_WRITE) ? COLOR_BREAK : COLOR_AMBER);
                fl_rectf(m_hex_x + hex_offset(c) - 1, yy + lh - 2, m_cw * 2 + 2, 2);
                fl_rectf(m_ascii_x + c * m_cw, yy + lh - 2, m_cw, 2);
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

int MemView::handle(int event)
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
                    /* Fl_Tooltip keeps the pointer: the text must outlive this call */
                    static std::string tip;
                    uint8_t b = 0;
                    debugger_read_memory(fz_debugger(), (hwaddr) a, 1, &b);
                    char head[24];
                    char ad[8];
                    hex16(ad, sizeof(ad), (unsigned) a);
                    snprintf(head, sizeof(head), "$%s = %u", ad, b);
                    tip = head;
                    if (const char* sym = debugger_get_symbol(fz_debugger(), (hwaddr) a)) {
                        tip += "   ";
                        tip += sym;
                    }
                    watchpoint_t wp;
                    if (debugger_find_watchpoint(fz_debugger(), (hwaddr) a, &wp)) {
                        char watch[24];
                        char line[64];
                        watch_range_text(watch, sizeof(watch), wp.addr, wp.end);
                        snprintf(line, sizeof(line), "\nBreak on %s: %s%s", watch_type_name(wp.type), watch,
                                 wp.disabled ? " (off)" : "");
                        tip += line;
                    }
                    Fl_Tooltip::enter_area(this, Fl::event_x() - x() - 4, Fl::event_y() - y() - 4, 8, 8, tip.c_str());
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

void MemView::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    clamp_top();
    sync_bar();
}

void MemView::geometry(int cw)
{
    m_cw = cw;
    m_hex_x = x() + 6 + cw * 6;
    m_ascii_x = m_hex_x + hex_offset(COLS) + cw;
}

void MemView::selection_changed()
{
    if (on_selection) on_selection();
}

void MemView::glyph(uint8_t b, char* ch) const
{
    if (cp437) {
        const unsigned cp = cp437_to_unicode(b) ? cp437_to_unicode(b) : ' ';
        const int n = fl_utf8encode(cp, ch);
        ch[n] = '\0';
    } else {
        ch[0] = isprint(b) ? (char) b : '.';
        ch[1] = '\0';
    }
}

void MemView::mark_matches(int start, int len, bool* out)
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

std::vector<uint8_t> MemView::selected_bytes() const
{
    std::vector<uint8_t> data((size_t) sel_len());
    if (!data.empty()) debugger_read_memory(fz_debugger(), (hwaddr) sel_lo(), (int) data.size(), data.data());
    return data;
}

void MemView::copy_text(const std::string& text)
{
    Fl::copy(text.c_str(), (int) text.size(), 1);
}

void MemView::copy_hex() const
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

void MemView::context_menu()
{
    const int lo = sel_lo();
    const int hi = sel_hi();
    const bool range = hi > lo;
    watchpoint_t watch = {};
    const bool watch_here = dbg_ready() && debugger_find_watchpoint(fz_debugger(), (hwaddr) lo, &watch);
    /* Radio marks when the selection is exactly a watched range */
    const bool exact = watch_here && watch.addr == (hwaddr) lo && watch.end == (hwaddr) hi;
    auto radio = [&](int type) { return FL_MENU_RADIO | (exact && watch.type == type ? FL_MENU_VALUE : 0); };
    /* String watches take the selection length (16 for a single byte) */
    const int str_len = range ? std::min(hi - lo + 1, MAX_STR) : DEFAULT_STR;
    char str_label[48];
    snprintf(str_label, sizeof(str_label), "str  ASCII string (%d)", str_len);
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
        { "Show in Disassembly", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Add to Watch", 0, nullptr, nullptr, FL_SUBMENU, 0, 0, 0, 0 },
        { WATCH_TYPES[0].label, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { WATCH_TYPES[1].label, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { WATCH_TYPES[2].label, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { WATCH_TYPES[3].label, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { str_label, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { WATCH_TYPES[5].label, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { WATCH_TYPES[6].label, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { WATCH_TYPES[7].label, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Break on Read", 0, nullptr, nullptr, radio(WATCHPOINT_READ), 0, 0, 0, 0 },
        { "Break on Write", 0, nullptr, nullptr, radio(WATCHPOINT_WRITE), 0, 0, 0, 0 },
        { "Break on Access", 0, nullptr, nullptr, radio(WATCHPOINT_RW), 0, 0, 0, 0 },
        { "Remove Break", 0, nullptr, nullptr, watch_here ? 0 : FL_MENU_INACTIVE, 0, 0, 0, 0 },
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
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
            /* Add to Watch: one item per WatchType, in order */
            watch_add_at((hwaddr) lo, (int) (picked - items) - 11, str_len);
            break;
        case 20:
        case 21:
        case 22: {
            const int pick = (int) (picked - items);
            watch_break_range((hwaddr) lo, (hwaddr) hi,
                              pick == 20 ? WATCHPOINT_READ : pick == 21 ? WATCHPOINT_WRITE : WATCHPOINT_RW);
            break;
        }
        case 23:
            watch_break_clear_at(watch.addr);
            break;
        default:
            break;
    }
    redraw();
}

int MemView::addr_at(int ex, int ey)
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

int MemView::key(int k, const char* text)
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

void MemView::clamp_top()
{
    const int rows = visible_rows();
    if (m_top > ROWS_TOTAL - rows) m_top = ROWS_TOTAL - rows;
    if (m_top < 0) m_top = 0;
}

} // namespace zui::debugger
