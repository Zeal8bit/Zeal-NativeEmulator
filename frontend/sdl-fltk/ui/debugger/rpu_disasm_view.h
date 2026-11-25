/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>

namespace zui::debugger {

/** Scrollable listing of the RPU program RAM (256 x 24-bit instructions). */
class RpuDisasmView : public Fl_Widget {
public:
    RpuDisasmView(int X, int Y, int W, int H);

    void set_scrollbar(Fl_Scrollbar* bar);
    void scroll_to(int top);
    void sync_scrollbar();
    void refresh();

    void draw() override;
    int handle(int event) override;
    void resize(int X, int Y, int W, int H) override;

private:
    int rows() const;
    int max_top() const;
    void clamp_top();

    Fl_Scrollbar* m_bar = nullptr;
    int m_top = 0;
};

} // namespace zui::debugger
