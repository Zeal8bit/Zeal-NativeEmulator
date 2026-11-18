/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <vector>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "tools/tool_tab.h"
#include "widgets/panel.h"

namespace zui::tools {

struct WindowDef {
    const char* id;
    const char* title;
    const char* key; /* config key prefix */
};

class ToolWindow : public Fl_Double_Window {
public:
    ToolWindow(const WindowDef& def);
    void resize(int X, int Y, int W, int H) override;
    void draw() override;
    void select(int index);
    void refresh(bool paused, bool force);
    void open();
    void close();
    void suspend();
    bool wanted() const { return m_wanted; }
    void capture();
    void reset();
    void save(rini_config* ini);

private:
    void load();
    static void cb_tab(Fl_Widget* w, void* data);
    static void cb_close(Fl_Widget*, void* data);

    const WindowDef& m_def;
    std::vector<std::string> m_names;
    std::vector<ToolTab*> m_tabs;
    std::vector<widgets::Panel*> m_views;
    int m_current = 0;
    double m_last = 0;
    bool m_wanted = false;
    int m_x = -1, m_y = -1, m_w = 0, m_h = 0;
};

} // namespace zui::tools
