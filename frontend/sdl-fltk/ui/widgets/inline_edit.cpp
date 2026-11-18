/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/inline_edit.h"
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>

namespace zui::widgets {

InlineEdit::InlineEdit(int X, int Y, int W, int H)
    : Fl_Input(X, Y, W, H)
{}

int InlineEdit::handle(int event)
{
    if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape) {
        hide();
        if (parent()) parent()->redraw();
        return 1;
    }
    if (event == FL_UNFOCUS) {
        const int r = Fl_Input::handle(event);
        hide();
        if (parent()) parent()->redraw();
        return r;
    }
    return Fl_Input::handle(event);
}

} // namespace zui::widgets
