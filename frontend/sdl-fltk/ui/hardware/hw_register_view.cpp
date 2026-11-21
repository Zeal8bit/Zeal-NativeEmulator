/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hardware/hw_register_view.h"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Tooltip.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/navigation.h"
#include "frontend/sdl-fltk/bridge/hw.h"
#include "memtools/pattern.h"
#include "theme/theme.h"
#include "widgets/panel.h"

namespace zui::hardware {

using namespace zui::common;
using namespace zui::debugger;
using namespace zui::memtools;
using namespace zui::theme;
using namespace zui::widgets;

HwRegisterView::HwRegisterView(int X, int Y, int W, int H, const char* title)
    : Panel(X, Y, W, H, title), m_title(title)
{
    end();
}

void HwRegisterView::refresh(bool paused, bool force)
{
    m_items.clear();
    build();
    const bool snapshot = paused && force;
    std::map<std::string, std::string> next;
    std::string section;
    for (Item& it : m_items) {
        if (it.kind == Item::Section) {
            section = it.label;
            continue;
        }
        const std::string key = section + "/" + it.label;
        if (snapshot) {
            const std::string value = fingerprint(it);
            auto prev = m_prev.find(key);
            it.changed = m_have_prev && prev != m_prev.end() && prev->second != value;
            next[key] = value;
        } else {
            it.changed = paused && m_changed_keys.count(key) != 0;
        }
    }
    if (snapshot) {
        m_prev.swap(next);
        m_have_prev = true;
        m_changed_keys.clear();
        section.clear();
        for (const Item& it : m_items) {
            if (it.kind == Item::Section) section = it.label;
            else if (it.changed) m_changed_keys.insert(section + "/" + it.label);
        }
    }
    extra(paused ? "PAUSED" : "LIVE");
    redraw();
}

void HwRegisterView::draw()
{
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    layout();
    const int lh = data_line_height();
    const int asc = text_baseline(lh);
    const int cw = char_width();
    const int body_y = top();
    const int body_h = h() - HEADER_H;
    fl_push_clip(x(), body_y, w(), body_h);
    const int oy = body_y - m_scroll;

    for (int c = 0; c < m_cols; c++) {
        const int cx = col_x(c);
        fl_font(FL_HELVETICA, 10);
        fl_color(COLOR_DIM);
        fl_draw("HEX", cx + cw * HEX_COL, oy + PAD + 10);
        fl_draw("DEC", cx + cw * DEC_COL, oy + PAD + 10);
    }

    for (size_t i = 0; i < m_items.size(); i++) {
        const Item& it = m_items[i];
        const int cx = col_x(it.col);
        const int ry = oy + it.ry;
        if (ry > body_y + body_h || ry + lh * 2 < body_y) continue;
        if (it.kind == Item::Section) {
            fl_font(FL_HELVETICA_BOLD, 10);
            fl_color(COLOR_AMBER);
            fl_draw(it.label.c_str(), cx, ry + SECTION_H - 8);
            const int tw = (int) fl_width(it.label.c_str());
            fl_color(COLOR_BORDER);
            fl_xyline(cx + tw + 6, ry + SECTION_H - 12, cx + m_col_w);
            continue;
        }
        if ((int) i == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(cx - 4, ry, m_col_w + 8, lh);
        }
        fl_font(DATA_FONT, DATA_SIZE);
        fl_color(COLOR_DIM);
        fl_draw(it.label.c_str(), cx, ry + asc);
        if (it.kind == Item::Chips) {
            int chx = cx + cw * HEX_COL;
            for (const Chip& ch : it.chips) {
                const int chw = (int) fl_width(ch.name.c_str()) + cw;
                if (ch.on) {
                    fl_color(it.changed ? COLOR_CHANGED : COLOR_GREEN);
                    fl_rectf(chx, ry + 1, chw, lh - 2);
                    fl_color(COLOR_BG);
                } else {
                    fl_color(it.changed ? COLOR_CHANGED : COLOR_BORDER);
                    fl_rect(chx, ry + 1, chw, lh - 2);
                    fl_color(COLOR_DIM);
                }
                fl_draw(ch.name.c_str(), chx + cw / 2, ry + asc);
                chx += chw + 3;
            }
            continue;
        }
        if (it.kind == Item::Swatches) {
            int sx = cx + cw * HEX_COL;
            const int sz = lh - 2;
            const int fit = (m_col_w - cw * HEX_COL) / (sz + 2);
            for (size_t k = 0; k < it.colors.size() && (int) k < fit; k++) {
                const uint16_t c = it.colors[k];
                const uchar r = (uchar) (((c >> 11) & 0x1F) * 255 / 31);
                const uchar g = (uchar) (((c >> 5) & 0x3F) * 255 / 63);
                const uchar b = (uchar) ((c & 0x1F) * 255 / 31);
                fl_color(fl_rgb_color(r, g, b));
                fl_rectf(sx, ry + 1, sz, sz);
                fl_color(it.changed ? COLOR_CHANGED : COLOR_BORDER);
                fl_rect(sx, ry + 1, sz, sz);
                sx += sz + 2;
            }
            continue;
        }
        const Fl_Color value_color = it.changed ? COLOR_CHANGED : it.has_addr ? COLOR_CYAN : COLOR_TEXT;
        fl_color(value_color);
        if (!it.hex.empty()) fl_draw(it.hex.c_str(), cx + cw * HEX_COL, ry + asc);
        if (!it.dec.empty()) fl_draw(it.dec.c_str(), cx + cw * DEC_COL, ry + asc);
        int ix = cx + cw * INFO_COL;
        if (it.hex.empty() && it.dec.empty()) ix = cx + cw * HEX_COL;
        if (!it.info.empty()) {
            fl_color(it.changed ? COLOR_CHANGED : (it.hex.empty() && it.dec.empty()) ? COLOR_TEXT : COLOR_AMBER);
            fl_push_clip(ix, ry, cx + m_col_w - ix, lh);
            fl_draw(it.info.c_str(), ix, ry + asc);
            fl_pop_clip();
            ix += (int) fl_width(it.info.c_str()) + cw;
        }
        if (it.glyph >= 0) {
            draw_glyph(it.glyph, it.duty, ix, ry + 2, cw * 4, lh - 4, it.changed ? COLOR_CHANGED : COLOR_CYAN);
            ix += cw * 5;
        }
        if (it.level >= 0) {
            const int bw = cw * 8;
            fl_color(COLOR_BORDER);
            fl_rect(ix, ry + 3, bw, lh - 6);
            fl_color(COLOR_GREEN);
            const float lv = it.level > 1.0f ? 1.0f : it.level;
            fl_rectf(ix + 1, ry + 4, (int) ((bw - 2) * lv), lh - 8);
        }
    }

    /* Scroll indicator */
    if (m_content_h > body_h) {
        const int track = body_h - 4;
        const int thumb = track * body_h / m_content_h;
        const int ty = body_y + 2 + (track - thumb) * m_scroll / (m_content_h - body_h);
        fl_color(COLOR_BORDER);
        fl_rectf(x() + w() - 5, ty, 3, thumb);
    }
    fl_pop_clip();
    draw_header(x(), y(), w(), m_title, extra_text());
}

int HwRegisterView::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            set_hover(-1);
            return 1;
        case FL_MOVE:
            set_hover(item_at(Fl::event_x(), Fl::event_y()));
            return 1;
        case FL_MOUSEWHEEL: {
            const int body_h = h() - HEADER_H;
            int s = m_scroll + Fl::event_dy() * data_line_height() * 3;
            const int max = m_content_h > body_h ? m_content_h - body_h : 0;
            if (s > max) s = max;
            if (s < 0) s = 0;
            if (s != m_scroll) {
                m_scroll = s;
                m_hover = -1;
                redraw();
            }
            return 1;
        }
        case FL_PUSH: {
            const int i = item_at(Fl::event_x(), Fl::event_y());
            if (i < 0) return 1;
            const Item& it = m_items[(size_t) i];
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                context_menu(it);
            } else if (it.has_addr) {
                goto_memory(it.addr);
            }
            return 1;
        }
        default:
            break;
    }
    return Panel::handle(event);
}

void HwRegisterView::resize(int X, int Y, int W, int H)
{
    Panel::resize(X, Y, W, H);
    clamp_scroll();
}

void HwRegisterView::section(const char* name)
{
    Item it;
    it.kind = Item::Section;
    it.label = name;
    m_items.push_back(it);
}

HwRegisterView::Item& HwRegisterView::row(const char* label, const std::string& hex, const std::string& dec, const std::string& info)
{
    Item it;
    it.label = label;
    it.hex = hex;
    it.dec = dec;
    it.info = info;
    m_items.push_back(it);
    return m_items.back();
}

HwRegisterView::Item& HwRegisterView::u8(const char* label, unsigned v, const std::string& info)
{
    return row(label, h8(v), dec(v), info);
}

HwRegisterView::Item& HwRegisterView::u16(const char* label, unsigned v, const std::string& info)
{
    return row(label, h16(v), dec(v), info);
}

HwRegisterView::Item& HwRegisterView::addr16(const char* label, unsigned v, const std::string& info)
{
    Item& it = row(label, h16(v), dec(v), info);
    it.has_addr = true;
    it.addr = v & 0xFFFF;
    return it;
}

HwRegisterView::Item& HwRegisterView::phys(const char* label, uint32_t v, const std::string& info)
{
    Item& it = row(label, hexn(v, 6), "", info);
    const int32_t virt = fz_hw_phys_to_virt(v);
    if (virt >= 0) {
        it.has_addr = true;
        it.addr = (hwaddr) virt;
        if (it.info.empty()) it.info = "virt " + h16((unsigned) virt);
    }
    return it;
}

HwRegisterView::Item& HwRegisterView::text(const char* label, const std::string& info)
{
    return row(label, "", "", info);
}

HwRegisterView::Item& HwRegisterView::chips(const char* label, std::vector<Chip> list)
{
    Item it;
    it.kind = Item::Chips;
    it.label = label;
    it.chips = std::move(list);
    m_items.push_back(it);
    return m_items.back();
}

HwRegisterView::Item& HwRegisterView::swatches(const char* label, const uint16_t* colors, int count)
{
    Item it;
    it.kind = Item::Swatches;
    it.label = label;
    it.colors.assign(colors, colors + count);
    m_items.push_back(it);
    return m_items.back();
}

std::string HwRegisterView::h8(unsigned v)
{
    char t[8];
    hex8(t, sizeof(t), v);
    return std::string("$") + t;
}

std::string HwRegisterView::h16(unsigned v)
{
    char t[8];
    hex16(t, sizeof(t), v);
    return std::string("$") + t;
}

std::string HwRegisterView::hexn(uint32_t v, int digits)
{
    char t[16];
    snprintf(t, sizeof(t), config.debugger.hex_upper ? "$%0*X" : "$%0*x", digits, (unsigned) v);
    return t;
}

std::string HwRegisterView::dec(long long v)
{
    char t[24];
    snprintf(t, sizeof(t), "%lld", v);
    return t;
}

std::string HwRegisterView::fmt(const char* format, ...)
{
    char t[128];
    va_list args;
    va_start(args, format);
    vsnprintf(t, sizeof(t), format, args);
    va_end(args);
    return t;
}

const char* HwRegisterView::name_of(const char* const* names, int count, int index)
{
    return index >= 0 && index < count ? names[index] : "?";
}

std::string HwRegisterView::fingerprint(const Item& it)
{
    std::string s = it.hex + "|" + it.dec + "|" + it.info;
    for (const Chip& c : it.chips) s += c.on ? '1' : '0';
    for (uint16_t c : it.colors) s += std::to_string(c) + ",";
    return s;
}

int HwRegisterView::col_x(int c) const
{ return x() + 12 + c * (m_col_w + GAP); }

void HwRegisterView::layout()
{
    const int lh = data_line_height();
    const int cw = char_width();
    const int avail = w() - 24;
    const int min_w = cw * MIN_COL_CHARS;
    m_cols = (avail + GAP) / (min_w + GAP);
    if (m_cols < 1) m_cols = 1;
    if (m_cols > 3) m_cols = 3;
    m_col_w = (avail - (m_cols - 1) * GAP) / m_cols;
    if (m_col_w < min_w && m_cols == 1) m_col_w = avail > 0 ? avail : min_w;

    /* Sections stay in reading order, filling columns top to bottom
     * and moving on once a column reaches its share of the total. */
    auto item_h = [&](const Item& it) { return it.kind == Item::Section ? SECTION_H : lh; };
    int total = 0;
    for (const Item& it : m_items) total += item_h(it);
    const int share = total / m_cols;
    std::vector<int> heights(m_cols, PAD + 14);
    int col = 0;
    size_t i = 0;
    while (i < m_items.size()) {
        size_t end = i + 1;
        while (end < m_items.size() && m_items[end].kind != Item::Section) end++;
        int sh = 0;
        for (size_t k = i; k < end; k++) sh += item_h(m_items[k]);
        const int used = heights[col] - (PAD + 14);
        if (col < m_cols - 1 && used > 0 && used + sh / 2 > share) col++;
        int yy = heights[col];
        for (size_t k = i; k < end; k++) {
            m_items[k].col = col;
            m_items[k].ry = yy;
            yy += item_h(m_items[k]);
        }
        heights[col] = yy + 4;
        i = end;
    }
    m_content_h = PAD;
    for (int hgt : heights) {
        if (hgt > m_content_h) m_content_h = hgt;
    }
    m_content_h += PAD;
    clamp_scroll();
}

void HwRegisterView::clamp_scroll()
{
    const int body_h = h() - HEADER_H;
    const int max = m_content_h > body_h ? m_content_h - body_h : 0;
    if (m_scroll > max) m_scroll = max;
    if (m_scroll < 0) m_scroll = 0;
}

int HwRegisterView::item_at(int ex, int ey) const
{
    if (ey < top()) return -1;
    const int lh = data_line_height();
    const int cy = ey - top() + m_scroll;
    for (size_t i = 0; i < m_items.size(); i++) {
        const Item& it = m_items[i];
        if (it.kind == Item::Section) continue;
        const int cx = col_x(it.col);
        if (ex >= cx - 4 && ex < cx + m_col_w + 4 && cy >= it.ry && cy < it.ry + lh) return (int) i;
    }
    return -1;
}

void HwRegisterView::set_hover(int i)
{
    if (i == m_hover) return;
    m_hover = i;
    redraw();
    if (i < 0) return;
    const Item& it = m_items[(size_t) i];
    if (it.has_addr || !it.tip.empty()) {
        static std::string tip;
        tip = it.tip;
        if (it.has_addr) tip += (tip.empty() ? "" : "\n") + std::string("Click: show in memory");
        Fl_Tooltip::enter_area(this, col_x(it.col) - x(), top() + it.ry - m_scroll - y(), m_col_w,
                               data_line_height(), tip.c_str());
    }
}

void HwRegisterView::context_menu(const Item& it)
{
    const int addr_only = it.has_addr ? 0 : FL_MENU_INACTIVE;
    Fl_Menu_Item items[] = {
        { "Copy Value", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Show in Memory", 0, nullptr, nullptr, addr_only, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) return;
    if (picked == &items[1]) {
        goto_memory(it.addr);
        return;
    }
    std::string value = !it.hex.empty() ? it.hex : !it.dec.empty() ? it.dec : it.info;
    if (it.kind == Item::Chips) {
        for (const Chip& c : it.chips) {
            if (c.on) value += (value.empty() ? "" : " ") + c.name;
        }
    } else if (it.kind == Item::Swatches) {
        for (uint16_t c : it.colors) value += (value.empty() ? "" : " ") + h16(c);
    }
    Fl::copy(value.c_str(), (int) value.size(), 1);
}

void HwRegisterView::draw_glyph(int wave, int duty, int gx, int gy, int gw, int gh, Fl_Color color)
{
    fl_color(color);
    const int top_y = gy, bot = gy + gh - 1, mid = gx + gw / 2;
    switch (wave) {
        case 0: { /* square, duty in eighths */
            const int hi = gx + (gw / 2) * (duty < 1 ? 1 : duty > 8 ? 8 : duty) / 8;
            fl_begin_line();
            fl_vertex(gx, bot); fl_vertex(gx, top_y); fl_vertex(hi, top_y); fl_vertex(hi, bot);
            fl_vertex(mid, bot); fl_vertex(mid, top_y);
            fl_vertex(mid + (hi - gx), top_y); fl_vertex(mid + (hi - gx), bot); fl_vertex(gx + gw, bot);
            fl_end_line();
            break;
        }
        case 1: /* triangle */
            fl_begin_line();
            fl_vertex(gx, bot); fl_vertex(gx + gw / 4, top_y); fl_vertex(mid, bot);
            fl_vertex(gx + 3 * gw / 4, top_y); fl_vertex(gx + gw, bot);
            fl_end_line();
            break;
        case 2: /* sawtooth */
            fl_begin_line();
            fl_vertex(gx, bot); fl_vertex(mid, top_y); fl_vertex(mid, bot);
            fl_vertex(gx + gw, top_y); fl_vertex(gx + gw, bot);
            fl_end_line();
            break;
        default: { /* noise */
            static const int pts[] = { 3, 0, 4, 1, 4, 0, 2, 4, 1, 3, 0, 4, 2 };
            fl_begin_line();
            const int n = (int) (sizeof(pts) / sizeof(pts[0]));
            for (int k = 0; k < n; k++) fl_vertex(gx + gw * k / (n - 1), top_y + (gh - 1) * pts[k] / 4);
            fl_end_line();
            break;
        }
    }
}

} // namespace zui::hardware
