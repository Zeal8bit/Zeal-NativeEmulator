/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/icon_button.h"
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>
#include "theme/theme.h"

namespace zui::widgets {

using namespace zui::theme;

IconButton::IconButton(int X, int Y, int W, int H, Icon icon, const char* text)
    : Fl_Button(X, Y, W, H), m_icon(icon), m_text(text)
{
    box(FL_FLAT_BOX);
    clear_visible_focus();
}

void IconButton::draw()
{
    const bool down = value() != 0;
    const bool hover = Fl::belowmouse() == this;
    fl_color(down ? COLOR_BORDER : hover ? COLOR_RAISED : COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    if (hover || down) {
        fl_color(COLOR_BORDER);
        fl_rect(x(), y(), w(), h());
    }
    const Fl_Color c = active_r() ? (m_color ? m_color : COLOR_TEXT) : COLOR_DIM;
    const int s = h() - 10;
    draw_icon(m_icon, x() + 6, y() + 5, s, c);
    if (m_text) {
        fl_font(FL_HELVETICA, 12);
        fl_color(active_r() ? COLOR_TEXT : COLOR_DIM);
        fl_draw(m_text, x() + s + 12, y() + (h() + fl_height()) / 2 - fl_descent());
    }
}

int IconButton::handle(int event)
{
    if (event == FL_ENTER || event == FL_LEAVE) {
        redraw();
        return 1;
    }
    return Fl_Button::handle(event);
}

} // namespace zui::widgets
