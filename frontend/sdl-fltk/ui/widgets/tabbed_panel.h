/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Several panels sharing one place, chosen with tabs drawn in the header
 * ("WATCH 4   BREAKPOINTS 2"). Each tab is a Panel shown without its own
 * header; its title and extra text make the tab label. */
#pragma once

#include <functional>
#include <vector>
#include "widgets/panel.h"

namespace zui::widgets {

class TabbedPanel : public Panel {
public:
    TabbedPanel(int X, int Y, int W, int H);

    /** Add a panel created inside this group (between begin() and end()). */
    void add_tab(Panel* panel);
    int current() const { return m_current; }
    void select(int index);
    /** The user picked another tab. */
    std::function<void(int)> on_select;

    void refresh(bool paused, bool force) override;
    void resize(int X, int Y, int W, int H) override;
    int handle(int event) override;
    void set_collapsed(bool collapsed) override;

protected:
    void draw_header_content(int x, int y, int w) override;

private:
    void tab_rect(int index, int& tx, int& tw) const;
    int tab_at(int ex, int ey) const;
    void label(int index, char* out, size_t size) const;

    std::vector<Panel*> m_tabs;
    int m_current = 0;
};

} // namespace zui::widgets
