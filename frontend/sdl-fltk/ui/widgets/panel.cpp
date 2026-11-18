/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/panel.h"
#include <cstdio>
#include <cstring>
#include <FL/Fl_Group.H>
#include "theme/theme.h"

namespace zui::widgets {

using namespace zui::theme;

Panel::Panel(int X, int Y, int W, int H, const char* title)
    : Fl_Group(X, Y, W, H), m_title(title)
{
    box(FL_FLAT_BOX);
    color(COLOR_SURFACE);
}

int Panel::top() const
{ return y() + HEADER_H; }

void Panel::extra(const char* text)
{
    if (strcmp(m_extra, text) == 0) return;
    snprintf(m_extra, sizeof(m_extra), "%s", text);
    damage(FL_DAMAGE_ALL, x(), y(), w(), HEADER_H);
}

void Panel::draw()
{
    Fl_Group::draw();
    draw_header(x(), y(), w(), m_title, m_extra);
}

} // namespace zui::widgets
