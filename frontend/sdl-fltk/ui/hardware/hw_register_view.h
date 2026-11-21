/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "common/machine.h"
#include "widgets/panel.h"

namespace zui::hardware {

class HwRegisterView : public widgets::Panel {
public:
    struct Chip {
        std::string name;
        bool on;
    };

    struct Item {
        enum Kind { Section, Row, Chips, Swatches } kind = Row;
        std::string label, hex, dec, info, tip;
        bool has_addr = false;
        hwaddr addr = 0;
        std::vector<Chip> chips;
        std::vector<uint16_t> colors;
        int glyph = -1;      /* waveform glyph (WAVE_*), -1 none */
        int duty = 4;        /* square: high time in eighths (1-8) */
        float level = -1.0f; /* level bar 0..1, < 0 none */
        bool changed = false;
        /* layout (content coordinates) */
        int col = 0;
        int ry = 0;
    };

    HwRegisterView(int X, int Y, int W, int H, const char* title);
    void refresh(bool paused, bool force) override;
    void draw() override;
    int handle(int event) override;
    void resize(int X, int Y, int W, int H) override;

protected:
    /** Append the sections/rows for the current machine state. */
    virtual void build() = 0;

    /* -------- builders -------- */

    void section(const char* name);
    Item& row(const char* label, const std::string& hex, const std::string& dec, const std::string& info = "");
    Item& u8(const char* label, unsigned v, const std::string& info = "");
    Item& u16(const char* label, unsigned v, const std::string& info = "");

    /** 16-bit virtual address: clickable (Show in Memory). */
    Item& addr16(const char* label, unsigned v, const std::string& info = "");

    /** 24-bit physical address: clickable when mapped by the MMU. */
    Item& phys(const char* label, uint32_t v, const std::string& info = "");
    Item& text(const char* label, const std::string& info);
    Item& chips(const char* label, std::vector<Chip> list);
    Item& swatches(const char* label, const uint16_t* colors, int count);

    /* -------- formatting -------- */

    static std::string h8(unsigned v);
    static std::string h16(unsigned v);
    static std::string hexn(uint32_t v, int digits);
    static std::string dec(long long v);

    static std::string fmt(const char* format, ...)
#if defined(__GNUC__)
        __attribute__((format(printf, 1, 2)))
#endif
        ;

    /** "name" from a table, or "?" when out of range. */
    static const char* name_of(const char* const* names, int count, int index);

private:
    static constexpr int PAD = 6;
    static constexpr int SECTION_H = 24;
    static constexpr int GAP = 24;
    static constexpr int HEX_COL = 14;
    static constexpr int DEC_COL = 24;
    static constexpr int INFO_COL = 31;
    static constexpr int MIN_COL_CHARS = 44;

    static std::string fingerprint(const Item& it);
    int col_x(int c) const;
    void layout();
    void clamp_scroll();
    int item_at(int ex, int ey) const;
    void set_hover(int i);
    void context_menu(const Item& it);
    static void draw_glyph(int wave, int duty, int gx, int gy, int gw, int gh, Fl_Color color);

    const char* m_title;
    std::vector<Item> m_items;
    std::map<std::string, std::string> m_prev;
    std::set<std::string> m_changed_keys;
    bool m_have_prev = false;
    int m_scroll = 0;
    int m_content_h = 0;
    int m_cols = 1;
    int m_col_w = 300;
    int m_hover = -1;
};

} // namespace zui::hardware
