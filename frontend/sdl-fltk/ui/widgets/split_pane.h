/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** Two children separated by a draggable divider. One side keeps a fixed
 *  size (in pixels) while the other takes the remaining space. */

#pragma once

#include <functional>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>

namespace zui::widgets {

class SplitPane : public Fl_Group {
public:
    static constexpr int GAP = 5;

    SplitPane(int x, int y, int w, int h, bool horizontal, bool fixed_second);
    void set(Fl_Widget* a, Fl_Widget* b, int fixed_size, int min_a = 80, int min_b = 80);
    /** Collapsed: only the first child is shown, filling the pane. */
    void collapse(bool collapsed);
    bool collapsed() const { return m_collapsed; }
    /** The second child keeps only `size` pixels (e.g. a collapsed panel's
     *  header) and the first takes the rest; 0 = normal split. No dragging. */
    void squeeze_second(int size);
    void fixed_size(int size);
    int fixed_size() const { return m_fixed; }
    void on_change(std::function<void()> cb) { m_changed = std::move(cb); }
    void resize(int x, int y, int w, int h) override;
    int handle(int event) override;
    void draw() override;
    void layout();

private:
    int extent() const;
    bool over_divider(int ex, int ey) const;

    Fl_Widget* m_a = nullptr;
    Fl_Widget* m_b = nullptr;
    bool m_horizontal;
    bool m_fixed_second;
    bool m_collapsed = false;
    int m_squeeze = 0;
    bool m_dragging = false;
    bool m_cursor_set = false;
    int m_fixed = 200;
    int m_min_a = 80;
    int m_min_b = 80;
    std::function<void()> m_changed;
};

} // namespace zui::widgets
