/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"

namespace zui::symbols {

struct Symbol {
    std::string name;
    std::string lower;  /* for case-insensitive filtering */
    hwaddr addr;
    uint8_t kind;       /* symbol_kind_t */
    bool local;
};

class SymbolList : public Fl_Widget {
public:
    enum class Sort { Name, Kind, Address };
    /** Kind filter value showing every kind (else a symbol_kind_t). */
    static constexpr int KIND_ALL = -1;

    SymbolList(int X, int Y, int W, int H, Fl_Scrollbar* bar);

    /** Reload from the debugger if the symbol table changed. */
    bool sync();
    void filter(const char* text);

    /** Show only one symbol_kind_t, or KIND_ALL. */
    void filter_kind(int kind);

    /** Sort by `key`; picking the current key again flips the direction. */
    void sort_by(Sort key);
    size_t total() const { return m_all.size(); }
    size_t shown() const { return m_rows.size(); }
    bool filtered() const;
    void scroll_to(int row);
    void sync_bar();
    void resize(int X, int Y, int W, int H) override;
    void draw() override;
    int handle(int event) override;
    void on_change(std::function<void()> cb) { m_changed = std::move(cb); }

private:
    int row_h() const;
    int visible_rows() const;
    int row_at(int ey) const;
    void set_hover(int r);

    /** Kind column follows the longest shown name, within the widget. */
    int kind_x() const;
    int addr_x() const;

    static constexpr int KIND_CHARS = 8; /* "CONST" plus a gap */

    void draw_columns();
    void draw_column(const char* label, int cx, bool active);
    void apply();
    void context_menu(const Symbol& s);

    Fl_Scrollbar* m_bar;
    std::vector<Symbol> m_all;
    std::vector<int> m_rows;   /* indices into m_all, filtered and sorted */
    uint64_t m_generation = UINT64_MAX;
    std::string m_filter;
    int m_kind = KIND_ALL;
    Sort m_sort = Sort::Address;
    bool m_ascending = true;
    size_t m_name_chars = 0;
    int m_top = 0;
    int m_hover = -1;
    std::function<void()> m_changed;
};

} // namespace zui::symbols
