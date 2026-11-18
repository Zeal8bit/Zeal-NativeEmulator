/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** Main window body: either the screen alone, or the debugger's layout
 *  (which contains the same screen widget). */

#pragma once

#include <functional>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>

namespace zui::widgets {

class DockLayout : public Fl_Group {
public:
    DockLayout(int x, int y, int w, int h);
    /** Root widget of the layout; `expand(bool)` shows/hides the debugger. */
    void set_root(Fl_Widget* root, std::function<void(bool)> expand);
    void docks_visible(bool visible);
    bool docks_visible() const { return m_docks; }
    void resize(int x, int y, int w, int h) override;

private:
    Fl_Widget* m_root = nullptr;
    std::function<void(bool)> m_expand;
    bool m_docks = false;
};

} // namespace zui::widgets
