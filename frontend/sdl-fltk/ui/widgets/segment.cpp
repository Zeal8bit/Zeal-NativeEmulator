/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/segment.h"
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>
#include "theme/theme.h"

namespace zui::widgets {

using namespace zui::theme;

Segment::Segment(int X, int Y, int W, int H, const char* label)
    : Fl_Button(X, Y, W, H, label)
{
    type(FL_RADIO_BUTTON);
    box(FL_FLAT_BOX);
    clear_visible_focus();
}

void Segment::draw()
{
    const bool on = value() != 0;
    const bool hover = Fl::belowmouse() == this;
    fl_color(on ? COLOR_RAISED : hover ? COLOR_HOVER : COLOR_BG);
    fl_rectf(x(), y(), w(), h());
    if (on) {
        fl_color(COLOR_AMBER);
        fl_rectf(x(), y() + h() - 2, w(), 2);
    }
    fl_font(FL_HELVETICA, 12);
    fl_color(on ? COLOR_TEXT : COLOR_DIM);
    fl_draw(label(), x(), y(), w(), h(), FL_ALIGN_CENTER);
}

int Segment::handle(int event)
{
    if (event == FL_ENTER || event == FL_LEAVE) {
        redraw();
        return 1;
    }
    return Fl_Button::handle(event);
}

} // namespace zui::widgets
