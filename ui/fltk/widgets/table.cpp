// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/widgets/table.h"
#include <FL/Fl.H>
#include <FL/Fl_Table.H>
#include <algorithm>

void table_prepare(Fl_Table *t)
{
    // A filling box() type would repaint over Fl_Table's own scrollbars: it draws its
    // widget box after drawing its children.
    t->box(FL_DOWN_FRAME);
    t->table_box(FL_DOWN_BOX);
    t->color(FL_BACKGROUND2_COLOR);
    t->selection_color(FL_SELECTION_COLOR);
    t->col_header(1);
    t->col_resize(1);
    t->col_resize_min(24);
    t->row_header(0);
    t->row_resize(0);
    t->when(FL_WHEN_RELEASE);
}
// Fl_Table 1.4 does not translate wheel events itself.
void table_wheel(Fl_Table *t, int rows)
{
    if (rows > 0 && Fl::event_dy())
        t->row_position(std::clamp(t->row_position() + Fl::event_dy() * 3, 0, rows - 1));
}
