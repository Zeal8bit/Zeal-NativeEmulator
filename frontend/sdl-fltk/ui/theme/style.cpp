/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "theme/style.h"
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include "theme/theme.h"

namespace zui::theme {

void style_input(Fl_Input* in)
{
    in->textfont(DATA_FONT);
    in->textsize(DATA_SIZE);
    in->color(COLOR_BG);
    in->textcolor(COLOR_TEXT);
    in->cursor_color(COLOR_AMBER);
    in->selection_color(COLOR_SELECT);
    in->box(FL_BORDER_BOX);
}

void style_button(Fl_Button* b)
{
    b->labelsize(12);
    b->box(FL_FLAT_BOX);
    b->down_box(FL_FLAT_BOX);
    b->color(COLOR_RAISED);
    b->selection_color(COLOR_BORDER);
    b->labelcolor(COLOR_TEXT);
    b->clear_visible_focus();
}

void style_check(Fl_Check_Button* b)
{
    b->labelsize(12);
    b->labelcolor(COLOR_TEXT);
    b->selection_color(COLOR_AMBER);
    b->clear_visible_focus();
}

} // namespace zui::theme
