/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/dock_layout.h"
#include <functional>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include "theme/theme.h"

namespace zui::widgets {

using namespace zui::theme;

DockLayout::DockLayout(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h)
{
    box(FL_FLAT_BOX);
    color(COLOR_BG);
    end();
}

void DockLayout::set_root(Fl_Widget* root, std::function<void(bool)> expand)
{
    m_root = root;
    m_expand = std::move(expand);
    if (root->parent() != this) {
        add(root);
    }
    root->resize(x(), y(), w(), h());
    docks_visible(m_docks);
}

void DockLayout::docks_visible(bool visible)
{
    m_docks = visible && m_expand;
    if (m_expand) {
        m_expand(m_docks);
    }
    if (m_root) {
        m_root->resize(x(), y(), w(), h());
    }
    redraw();
}

void DockLayout::resize(int nx, int ny, int nw, int nh)
{
    Fl_Widget::resize(nx, ny, nw, nh);
    if (m_root) {
        m_root->resize(nx, ny, nw, nh);
    }
}

} // namespace zui::widgets
