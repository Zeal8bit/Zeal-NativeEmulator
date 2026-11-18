/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/filter_input.h"
#include <FL/Fl.H>
#include <FL/Fl_Input.H>

namespace zui::widgets {

FilterInput::FilterInput(int X, int Y, int W, int H)
    : Fl_Input(X, Y, W, H)
{}

int FilterInput::handle(int event)
{
    if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape && size() > 0) {
        value("");
        do_callback();
        return 1;
    }
    return Fl_Input::handle(event);
}

} // namespace zui::widgets
