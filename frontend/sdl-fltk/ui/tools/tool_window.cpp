/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tools/tool_window.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "app/main_window.h"
#include "common/clock.h"
#include "common/machine.h"
#include "common/persist.h"
#include "theme/theme.h"
#include "tools/tool_registry.h"
#include "tools/tool_tab.h"
#include "widgets/panel.h"

namespace zui::tools {

using namespace zui::app;
using namespace zui::common;
using namespace zui::theme;
using namespace zui::widgets;

constexpr int TOOL_BAR_H = 38;

ToolWindow::ToolWindow(const WindowDef& def)
    : Fl_Double_Window(900, 600, def.title), m_def(def)
{
    color(COLOR_BG);
    std::vector<ViewInfo> infos = registered_views()[def.id];
    std::stable_sort(infos.begin(), infos.end(),
                     [](const ViewInfo& a, const ViewInfo& b) { return a.order < b.order; });
    fl_font(FL_HELVETICA, 12);
    int tx = 8;
    /* Tabs keep pointers into m_names: never let it reallocate */
    m_names.reserve(infos.size());
    for (size_t i = 0; i < infos.size(); i++) {
        const int tw = (int) fl_width(infos[i].name.c_str()) + 28;
        m_names.push_back(infos[i].name);
        ToolTab* tab = new ToolTab(tx, 6, tw, TOOL_BAR_H - 10, m_names.back().c_str());
        tab->callback(cb_tab, (void*) (intptr_t) i);
        m_tabs.push_back(tab);
        tx += tw + 2;
    }
    for (const ViewInfo& info : infos) {
        Panel* view = info.factory(0, TOOL_BAR_H, w(), h() - TOOL_BAR_H);
        if (view->parent() != this) {
            add(view);
        }
        view->hide();
        m_views.push_back(view);
    }
    if (m_views.empty()) {
        Fl_Box* none = new Fl_Box(0, TOOL_BAR_H, w(), h() - TOOL_BAR_H, "No views yet");
        none->labelcolor(COLOR_DIM);
    }
    end();
    size_range(420, 260);
    set_non_modal();
    callback(cb_close, this);
    load();
    select(m_current);
}

void ToolWindow::resize(int X, int Y, int W, int H)
{
    Fl_Double_Window::resize(X, Y, W, H);
    for (Panel* v : m_views) v->resize(0, TOOL_BAR_H, W, H - TOOL_BAR_H);
}

void ToolWindow::draw()
{
    Fl_Double_Window::draw();
    fl_color(COLOR_BORDER);
    fl_xyline(0, TOOL_BAR_H - 1, w());
}

void ToolWindow::select(int index)
{
    if (m_views.empty()) {
        return;
    }
    if (index < 0 || index >= (int) m_views.size()) {
        index = 0;
    }
    m_current = index;
    for (size_t i = 0; i < m_views.size(); i++) {
        if ((int) i == index) {
            m_views[i]->show();
        } else {
            m_views[i]->hide();
        }
    }
    m_tabs[index]->setonly();
    refresh(fz_paused(), true);
    redraw();
}

void ToolWindow::refresh(bool paused, bool force)
{
    if (!shown() || m_views.empty()) {
        return;
    }
    const double now = now_seconds();
    if (!force && (paused || now - m_last < 0.25)) {
        return;
    }
    m_last = now;
    m_views[m_current]->refresh(paused, force);
}

void ToolWindow::open()
{
    m_wanted = true;
    show();
    refresh(fz_paused(), true);
}

void ToolWindow::close()
{
    capture();
    m_wanted = false;
    hide();
}

void ToolWindow::suspend()
{
    capture();
    hide();
}

void ToolWindow::capture()
{
    if (!shown()) {
        return;
    }
    m_x = x();
    m_y = y();
    m_w = w();
    m_h = h();
}

void ToolWindow::reset()
{
    m_x = m_y = -1;
    m_w = m_h = 0;
    m_wanted = false;
}

void ToolWindow::save()
{
    capture();
    set_preference(m_def.key, "x", m_x);
    set_preference(m_def.key, "y", m_y);
    set_preference(m_def.key, "width", m_w);
    set_preference(m_def.key, "height", m_h);
    set_preference(m_def.key, "shown", m_wanted);
    set_preference(m_def.key, "view", m_current);
}

void ToolWindow::load()
{
    m_x = preference(m_def.key, "x", -1);
    m_y = preference(m_def.key, "y", -1);
    m_w = preference(m_def.key, "width", 0);
    m_h = preference(m_def.key, "height", 0);
    m_current = preference(m_def.key, "view", 0);
    if (m_w > 200 && m_h > 150) {
        resize(m_x >= 0 ? m_x : x(), m_y >= 0 ? m_y : y(), m_w, m_h);
    }
}

void ToolWindow::cb_tab(Fl_Widget* w, void* data)
{
    static_cast<ToolWindow*>(w->window())->select((int) (intptr_t) data);
}

void ToolWindow::cb_close(Fl_Widget*, void* data)
{
    static_cast<ToolWindow*>(data)->close();
    if (main_window()) {
        main_window()->update_menus();
    }
}

} // namespace zui::tools
