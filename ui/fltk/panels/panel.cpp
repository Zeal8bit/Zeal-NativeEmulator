// SPDX-License-Identifier: Apache-2.0
// The Panel base class: the interface every panel implements.
#include "ui/fltk/panel.h"

Panel::Panel(dbg_ui_t *u, int id) : Fl_Flex(0, 0, 300, 240, Fl_Flex::VERTICAL), ui(u), id(id)
{
    box(FL_FLAT_BOX);
    gap(u->theme.spacing);
    begin();
}

void Panel::finish()
{
    end();
    hide();
}

void Panel::resize(int x, int y, int w, int h)
{
    gap(ui->theme.spacing);
    Fl_Flex::resize(x, y, w, h);
}
