// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/widgets/control_row.h"
#include "ui/fltk/widgets/text_input.h"
#include <FL/Fl.H>
#include <FL/Fl_Choice.H>
#include <FL/fl_draw.H>
#include <algorithm>

ControlRow::ControlRow(int row_height) : Fl_Flex(0, 0, 300, row_height, Fl_Flex::HORIZONTAL)
{
    begin();
}

TextInput *ControlRow::add_location(const char *initial, const char *tooltip)
{
    location_ = new TextInput(0, 0, 0, h());
    location_->value(initial);
    location_->when(FL_WHEN_ENTER_KEY_ALWAYS);
    location_->tooltip(tooltip);
    return location_;
}

Fl_Choice *ControlRow::add_choice(const char *items)
{
    choice_ = new Fl_Choice(0, 0, 0, h());
    choice_->add(items);
    choice_->value(0);
    return choice_;
}

void ControlRow::layout(int row_height, int pad, int font, int font_size)
{
    gap(pad);
    if (choice_) {
        // Width the choice to its widest item plus the drop-down arrow.
        fl_font(font, font_size);
        int widest = 0;
        for (const Fl_Menu_Item *m = choice_->menu(); m && m->text; ++m)
            widest = std::max(widest, int(fl_width(m->text)));
        fixed(choice_, widest + row_height + 2 * pad);
    }
}
