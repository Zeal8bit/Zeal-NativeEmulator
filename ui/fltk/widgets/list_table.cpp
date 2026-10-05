// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/widgets/list_table.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include "ui/fltk/widgets/table.h"
#include <FL/Fl.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Table.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <string>
#include <vector>

using zeal_ui::hex;
using zeal_ui::text;

ListTable::ListTable(ListSource *s, dbg_ui_t *u, int x, int y, int w, int h)
    : Fl_Table(x, y, w, h), source(s), ui(u)
{
    // Menu items hold pointers into this vector, so it must never reallocate.
    menu_actions.reserve(8);
    table_prepare(this);
    cols(source->column_count());
    rows(0);
    end();
}

int ListTable::list_rows() const
{
    return source->row_count();
}

int ListTable::list_columns() const
{
    return source->column_count();
}

void ListTable::sync()
{
    int rows = list_rows(), columns = list_columns(), widths[16] = {};
    source->column_widths(widths, w());
    if (columns != cols())
        cols(columns);
    if (rows != row_count_shown) {
        row_count_shown = rows;
        Fl_Table::rows(rows);
    }
    if (ui->theme.row_height != row_metric) {
        row_metric = ui->theme.row_height;
        col_header_height(row_metric);
        row_height_all(row_metric);
    }
    for (int c = 0; c < columns && c < 16; c++)
        if (col_width(c) != widths[c])
            col_width(c, widths[c]);
    if (sel_row >= rows)
        sel_row = rows ? rows - 1 : -1;
    if (sel_row < 0 && rows > 0)
        sel_row = 0;
    source->selection_changed(sel_row, rows);
    // Rows carry live values (PC, counters), so repaint every refresh like the old
    // hand-painted canvas did. A full repaint is what keeps it consistent.
    redraw();
}

void ListTable::reveal(int row)
{
    if (row < 0 || row >= list_rows())
        return;
    if (row < toprow || row > botrow)
        row_position(std::max(0, row - (botrow - toprow) / 2));
}

void ListTable::select_row(int row)
{
    int rows = list_rows();
    if (rows <= 0) {
        sel_row = -1;
        redraw();
        return;
    }
    sel_row = std::clamp(row, 0, rows - 1);
    reveal(sel_row);
    redraw();
}

void ListTable::go_to(int row)
{
    uint32_t addr = source->row_address(row);
    if (addr || source->navigates_to_zero())
        ui->navigate(addr);
}

void ListTable::context_menu(int row)
{
    if (row < 0 || row >= list_rows())
        return;
    menu_row = row;
    select_row(row);
    if (!menu) {
        // A box-less menu takes Fl_Menu_Button's standalone popup path; parent it to the
        // table so the table owns it, but keep it hidden so it is never drawn inline.
        Fl_Group *saved = Fl_Group::current();
        Fl_Group::current(nullptr);
        menu = new Fl_Menu_Button(0, 0, 0, 0);
        Fl_Group::current(saved);
        menu->box(FL_NO_BOX);
        add(menu);
        menu->hide();
    }
    menu->clear();
    menu_actions.clear();
    for (int i = 0; i < source->action_count(); i++) {
        menu_actions.push_back({this, source->action_id(i)});
        menu->add(source->action_label(i), 0, action_cb, &menu_actions.back());
    }
    menu->popup();
}

void ListTable::action_cb(Fl_Widget *, void *data)
{
    MenuAction *a = (MenuAction *)data;
    a->table->run_action(a->action);
}

void ListTable::run_action(int action)
{
    int row = menu_row;
    if (row < 0)
        return;
    switch (action) {
    case ListSource::ACT_GOTO: go_to(row); break;
    case ListSource::ACT_COPY: {
        std::string a = hex(source->row_address(row), 4);
        Fl::copy(a.c_str(), int(a.size()), 1);
        ui->message = "Copied $" + a;
        break;
    }
    default: source->run_action(action, row); break;
    }
    ui->last_refresh = 0;
    redraw();
}

void ListTable::remove_selected()
{
    if (sel_row < 0 || sel_row >= list_rows())
        return;
    menu_row = sel_row;
    run_action(ListSource::ACT_REMOVE);
}

void ListTable::draw_cell(TableContext context, int R, int C, int X, int Y, int W, int H)
{
    int pad = ui->theme.spacing;
    if (context == CONTEXT_STARTPAGE) {
        fl_font(ui->mono_font, ui->theme.mono_size);
        return;
    }
    if (context == CONTEXT_COL_HEADER) {
        fl_push_clip(X, Y, W, H);
        fl_color(ui->color("background"));
        fl_rectf(X, Y, W, H);
        fl_color(ui->color("border"));
        fl_rect(X, Y, W, H);
        text(ui, "muted", source->column_header(C), X + pad, Y + H - 6, false);
        fl_pop_clip();
        return;
    }
    if (context == CONTEXT_ENDPAGE) {
        const char *hint = source->empty_hint();
        if (hint && list_rows() == 0)
            text(ui, "muted", hint, tix + pad, tiy + row_metric - 6, false);
        return;
    }
    if (context != CONTEXT_CELL)
        return;
    fl_push_clip(X, Y, W, H);
    const char *role = "text";
    std::string value = source->cell_text(R, C, role);
    const char *background = R == sel_row ? "selection" : "surface";
    if (R != sel_row)
        if (const char *row_role = source->row_background(R))
            background = row_role;
    fl_color(ui->color(background));
    fl_rectf(X, Y, W, H);
    text(ui, role, value, X + pad, Y + H - 6);
    fl_pop_clip();
}

int ListTable::handle(int event)
{
    if (event == FL_MOUSEWHEEL) {
        table_wheel(this, list_rows());
        return 1;
    }
    if (event == FL_KEYBOARD) {
        int rows = list_rows();
        switch (Fl::event_key()) {
        case FL_Up: select_row(sel_row - 1); return 1;
        case FL_Down: select_row(sel_row + 1); return 1;
        case FL_Page_Up: select_row(sel_row - (botrow - toprow + 1)); return 1;
        case FL_Page_Down: select_row(sel_row + (botrow - toprow + 1)); return 1;
        case FL_Home: select_row(0); return 1;
        case FL_End: select_row(rows - 1); return 1;
        case FL_Delete:
        case FL_Enter:
        case ' ':
            menu_row = sel_row;
            run_action(source->keyboard_action());
            return 1;
        default: break;
        }
    }
    // Fl_Table takes focus in its FL_ENTER handler behind a guard that never passes,
    // so the table can never be typed into; take focus on click instead.
    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE)
        take_focus();
    if (event == FL_FOCUS || event == FL_UNFOCUS)
        redraw();
    int ret = Fl_Table::handle(event);
    if (event == FL_PUSH) {
        int row = -1, col = -1;
        ResizeFlag flag = RESIZE_NONE;
        if (cursor2rowcol(row, col, flag) == CONTEXT_CELL && row >= 0 && row < list_rows()) {
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                context_menu(row);
            } else {
                select_row(row);
                menu_row = row;
                if (Fl::event_clicks() || source->acts_on_single_click())
                    run_action(source->default_action());
            }
        }
        ui->last_refresh = 0;
        return 1;
    }
    return ret;
}

void ListTable::resize(int x, int y, int w, int h)
{
    Fl_Table::resize(x, y, w, h);
    sync();
}
