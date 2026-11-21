/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/mem_find_input.h"
#include <FL/Fl.H>
#include <FL/Fl_Input.H>

namespace zui::debugger {

MemFindInput::MemFindInput(int X, int Y, int W, int H)
    : Fl_Input(X, Y, W, H)
{}

int MemFindInput::handle(int event)
{
    if (event == FL_KEYBOARD) {
        const int k = Fl::event_key();
        if ((k == FL_Enter || k == FL_KP_Enter) && on_enter) {
            on_enter((Fl::event_state() & FL_SHIFT) == 0);
            return 1;
        }
        if (k == FL_Escape && on_escape) {
            on_escape();
            return 1;
        }
    }
    return Fl_Input::handle(event);
}

} // namespace zui::debugger
