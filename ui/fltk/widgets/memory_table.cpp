// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/widgets/memory_table.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include "ui/fltk/widgets/table.h"
#include <FL/Fl.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Table.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using zeal_ui::address;
using zeal_ui::glyph;
using zeal_ui::hex;
using zeal_ui::hex_digit;
using zeal_ui::text;


MemoryTable::MemoryTable(MemorySource *s, dbg_ui_t *u, int x, int y, int w, int h)
    : Fl_Table(x, y, w, h), source(s), ui(u)
{
    table_prepare(this);
    cols(3);
    rows(0);
    end();
}

int MemoryTable::cell_width() const
{
    fl_font(ui->mono_font, ui->theme.mono_size);
    return int(fl_width("0")) + 1;
}

int MemoryTable::byte_columns() const
{
    int cell = cell_width();
    return w() > cell * 76 ? 16 : 8;
}

bool MemoryTable::sync(size_t bytes)
{
    dbg_ui_t *u = ui;
    int columns = byte_columns(), row = u->theme.row_height, pad = u->theme.spacing,
        cell = cell_width();
    int address = 6 * cell + 2 * pad, hexw = columns * 3 * cell + pad, ascii = columns * cell + pad;
    bool columns_changed = columns != shown_columns;
    shown_columns = columns;
    int count = int((bytes + columns - 1) / columns);
    bool changed = false;
    if (count != rows()) {
        rows(count);
        changed = true;
    }
    // Rows are created with a default height, so metrics must be (re)applied when
    // the row count changes too, not only when the font metrics change.
    if (row != row_metric || address != address_width || hexw != hex_width || ascii != ascii_width ||
        count != row_count) {
        row_metric = row;
        row_count = count;
        address_width = address;
        hex_width = hexw;
        ascii_width = ascii;
        col_header_height(row);
        col_width(0, address);
        col_width(1, hexw);
        col_width(2, ascii);
        row_height_all(row);
        changed = true;
    }
    // The byte-per-row count re-flows the view, so the stored selection row is stale.
    if (columns_changed && cursor >= 0)
        set_cursor(cursor, edit_col, true);
    return changed;
}

bool MemoryTable::refresh_view(size_t bytes, uint32_t start, bool upper, bool cp437)
{
    bool changed = sync(bytes);
    if (start != shown_start || upper != shown_upper || cp437 != shown_cp437 ||
        source->memory_bytes() != shown) {
        shown_start = start;
        shown_upper = upper;
        shown_cp437 = cp437;
        shown = source->memory_bytes();
        changed = true;
    }
    if (cursor < 0 && !source->memory_bytes().empty())
        cursor = 0;
    else if (cursor >= int(source->memory_bytes().size()))
        cursor = source->memory_bytes().empty() ? -1 : int(source->memory_bytes().size()) - 1;
    // Repaint only when the bytes or the view actually changed. Repainting at
    // refresh rate regardless is what made selection flicker.
    return changed;
}

void MemoryTable::reset_scroll()
{
    if (rows() > 0 && row_position() != 0)
        row_position(0);
    hover = -1;
    nibble = 0;
    if (source->memory_bytes().empty()) {
        cursor = -1;
        return;
    }
    set_cursor(0, edit_col);
}
// Fl_Table's partial repaint only works when table_box() draws nothing: on any
// FL_DAMAGE_CHILD it fills the whole client area and then repaints just the damaged
// cells, which would erase every other cell. Any local change therefore has to be a
// full-table repaint; the visible table is small enough that this is cheap, and it is
// what keeps the selection and caret from flickering.
void MemoryTable::update_selection()
{
    if (anchor >= 0 && cursor >= 0 && shown_columns > 0) {
        sel_lo = std::min(anchor, cursor) / shown_columns;
        sel_hi = std::max(anchor, cursor) / shown_columns;
    } else
        sel_lo = sel_hi = -1;
    redraw();
}

void MemoryTable::set_cursor(int index, int column, bool extend)
{
    if (source->memory_bytes().empty() || shown_columns <= 0)
        return;
    cursor = std::clamp(index, 0, int(source->memory_bytes().size()) - 1);
    if (!extend || anchor < 0)
        anchor = cursor;
    edit_col = column == 2 ? 2 : 1;
    int row = cursor / shown_columns;
    if (row < toprow || row > botrow)
        row_position(std::max(0, row - (botrow - toprow) / 2));
    update_selection();
}

int MemoryTable::byte_at_event(int *col_out, int *row_out)
{
    int row = -1, col = -1, index = -1;
    ResizeFlag flag = RESIZE_NONE;
    if (cursor2rowcol(row, col, flag) == CONTEXT_CELL) {
        int columns = shown_columns > 0 ? shown_columns : byte_columns();
        if (col == 0)
            index = row * columns;
        else {
            int X, Y, W, H;
            if (find_cell(CONTEXT_CELL, row, col, X, Y, W, H) == 0) {
                int cell = cell_width(), pad = ui->theme.spacing;
                int pos = (Fl::event_x() - X - pad) / cell;
                // Clicks in the gap between bytes, or in the cell padding, snap to the
                // nearest byte so that any click inside the cell lands on a byte.
                int slot = std::clamp(col == 1 ? pos / 3 : pos, 0, columns - 1);
                index = row * columns + slot;
            }
        }
        if (index >= 0 && index >= int(source->memory_bytes().size()))
            index = -1;
    }
    if (col_out)
        *col_out = col;
    if (row_out)
        *row_out = row;
    return index;
}

bool MemoryTable::write_byte(int index, uint8_t value)
{
    dbg_ui_t *u = ui;
    if (index < 0 || index >= int(source->memory_bytes().size()))
        return false;
    if (!u->snapshot.paused) {
        u->message = "Pause the CPU to edit memory";
        return false;
    }
    uint32_t addr = source->memory_start() + uint32_t(index);
    if (debugger_memory_write(u->host.debugger, DBG_VIRTUAL, addr, &value, 1) != DBG_OK) {
        u->message = "Cannot write $" + hex(addr, 4);
        return false;
    }
    // Writes use device semantics, so read back what actually landed (ROM ignores
    // writes) instead of pretending the edit took effect.
    uint8_t actual = value;
    debugger_memory_read(u->host.debugger, DBG_VIRTUAL, addr, &actual, 1);
    source->memory_bytes()[index] = actual;
    if (shown.size() == source->memory_bytes().size())
        shown[index] = actual;
    u->message = actual == value ? "Wrote $" + hex(value, 2) + " to $" + hex(addr, 4)
                                 : "Write to $" + hex(addr, 4) + " ignored by the device";
    return true;
}

bool MemoryTable::key_event()
{
    int columns = shown_columns > 0 ? shown_columns : byte_columns(), key = Fl::event_key();
    if (columns <= 0 || source->memory_bytes().empty())
        return false;
    if (cursor < 0)
        cursor = 0;
    // Leave Command/Control shortcuts (copy, menus) to the rest of the UI.
    bool command = Fl::event_state(FL_COMMAND) != 0;
    bool extend = Fl::event_state(FL_SHIFT) != 0;
    if (command && key != FL_Home && key != FL_End)
        return false;
    if (!command && key == FL_Tab) {
        set_cursor(cursor, edit_col == 1 ? 2 : 1, extend);
        return true;
    }
    int step = 0;
    bool navigation = true;
    switch (key) {
    case FL_Left: step = -1; break;
    case FL_Right: step = 1; break;
    case FL_Up: step = -columns; break;
    case FL_Down: step = columns; break;
    case FL_Page_Up: step = -(botrow - toprow + 1) * columns; break;
    case FL_Page_Down: step = (botrow - toprow + 1) * columns; break;
    case FL_Home: step = command ? -cursor : -(cursor % columns); break;
    case FL_End:
        step = command ? int(source->memory_bytes().size()) - 1 - cursor : columns - 1 - cursor % columns;
        break;
    case FL_BackSpace: step = -1; break;
    default: navigation = false; break;
    }
    if (navigation) {
        if (step)
            set_cursor(cursor + step, edit_col, extend);
        nibble = 0;
        return true;
    }
    if (!Fl::event_length())
        return false;
    int ch = (unsigned char)Fl::event_text()[0];
    int digit = hex_digit(ch);
    if (edit_col == 1 && digit >= 0) {
        uint8_t old = source->memory_bytes()[cursor];
        uint8_t value =
            nibble == 0 ? uint8_t((digit << 4) | (old & 0x0f)) : uint8_t((old & 0xf0) | digit);
        if (!write_byte(cursor, value))
            return true;
        if (nibble == 0)
            nibble = 1;
        else {
            nibble = 0;
            set_cursor(cursor + 1, edit_col);
        }
        redraw();
        return true;
    }
    if (edit_col == 2 && ch >= 32 && ch < 127) {
        if (write_byte(cursor, uint8_t(ch)))
            set_cursor(cursor + 1, edit_col);
        return true;
    }
    return false;
}

int MemoryTable::handle(int event)
{
    if (event == FL_MOUSEWHEEL) {
        // Fl_Table 1.4 does not translate wheel events itself.
        if (rows() > 0 && Fl::event_dy())
            row_position(std::clamp(row_position() + Fl::event_dy() * 3, 0, rows() - 1));
        return 1;
    }
    if (event == FL_KEYBOARD && key_event())
        return 1;
    // Fl_Table relies on FL_ENTER to take focus, but Fl_Group::handle(FL_ENTER) always
    // returns 1, so that take_focus() call never runs and the table can never be typed
    // into. Take focus on click instead, which is also the native behaviour.
    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE)
        take_focus();
    // Show the cursor whenever we gain focus.
    if (event == FL_FOCUS && cursor >= 0)
        set_cursor(cursor, edit_col);
    int ret = Fl_Table::handle(event);
    if (event == FL_PUSH) {
        int column = -1, index = byte_at_event(&column);
        if (index >= 0)
            set_cursor(index, column == 2 ? 2 : 1, Fl::event_state(FL_SHIFT) != 0);
    } else if (event == FL_DRAG) {
        // Dragging moves the cursor; the anchor stays put to form the range.
        int column = -1, index = byte_at_event(&column), want_col = column == 2 ? 2 : 1;
        if (index >= 0 && (index != cursor || want_col != edit_col)) {
            cursor = index;
            edit_col = want_col;
            nibble = 0;
            update_selection();
        }
    } else if (event == FL_MOVE)
        update_hover();
    else if (event == FL_LEAVE && hover != -1) {
        hover = -1;
        redraw();
    } else if (event == FL_FOCUS || event == FL_UNFOCUS)
        // The caret is only drawn while focused.
        redraw();
    return ret;
}

void MemoryTable::resize(int x, int y, int w, int h)
{
    Fl_Table::resize(x, y, w, h);
    sync(source->memory_bytes().size());
}

void MemoryTable::update_hover()
{
    int index = byte_at_event();
    if (index == hover)
        return;
    hover = index;
    redraw();
}

void MemoryTable::draw_cell(TableContext context, int R, int C, int X, int Y, int W, int H)
{
    dbg_ui_t *u = ui;
    int columns = shown_columns > 0 ? shown_columns : byte_columns();
    int pad = u->theme.spacing, cell = cell_width();
    if (context == CONTEXT_STARTPAGE) {
        fl_font(u->mono_font, u->theme.mono_size);
        return;
    }
    if (context == CONTEXT_COL_HEADER) {
        fl_push_clip(X, Y, W, H);
        fl_color(u->color("background"));
        fl_rectf(X, Y, W, H);
        fl_color(u->color("border"));
        fl_rect(X, Y, W, H);
        fl_font(u->mono_font, u->theme.mono_size);
        fl_color(u->color("muted"));
        if (C == 0)
            fl_draw("Address", X + pad, Y + H - 6);
        else if (C == 1)
            for (int j = 0; j < columns; j++)
                fl_draw(hex(j, 2, u->upper).c_str(), X + pad + j * 3 * cell, Y + H - 6);
        else
            fl_draw("ASCII", X + pad, Y + H - 6);
        fl_pop_clip();
        return;
    }
    if (context != CONTEXT_CELL)
        return;
    fl_push_clip(X, Y, W, H);
    // Whole rows are highlighted, driven by our own byte range rather than Fl_Table's
    // column-based selection (which cannot express "this byte in both panes").
    bool selected = R >= sel_lo && R <= sel_hi;
    fl_color(u->color(selected ? "selection" : "surface"));
    fl_rectf(X, Y, W, H);
    fl_font(u->mono_font, u->theme.mono_size);
    size_t base = size_t(R) * columns;
    if (C == 0) {
        text(u, selected ? "text" : "muted", hex(source->memory_start() + (uint32_t)base, 4, u->upper), X + pad,
             Y + H - 6);
    } else if (C == 1) {
        for (int j = 0; j < columns && base + j < source->memory_bytes().size(); j++) {
            int hx = X + pad + j * 3 * cell, index = int(base + j);
            if (hover == index) {
                fl_color(u->color("selection"));
                fl_rectf(hx, Y, 2 * cell, H);
            }
            std::string digits = hex(source->memory_bytes()[index], 2, u->upper);
            if (cursor == index && edit_col == 1 && Fl::focus() == this) {
                // Block cursor over the nibble being typed, like a hex editor.
                int cx = hx + nibble * cell;
                fl_color(u->color("link"));
                fl_rectf(cx, Y, cell, H);
                text(u, "background", std::string(1, digits[nibble]), cx, Y + H - 6);
                text(u, "text", std::string(1, digits[1 - nibble]), hx + (1 - nibble) * cell, Y + H - 6);
            } else
                text(u, "text", digits, hx, Y + H - 6);
        }
    } else {
        for (int j = 0; j < columns && base + j < source->memory_bytes().size(); j++) {
            int ax = X + pad + j * cell, index = int(base + j);
            uint8_t value = source->memory_bytes()[index];
            if (hover == index) {
                fl_color(u->color("selection"));
                fl_rectf(ax, Y, cell, H);
            }
            bool caret = cursor == index && edit_col == 2 && Fl::focus() == this;
            if (caret) {
                fl_color(u->color("link"));
                fl_rectf(ax, Y, cell, H);
            }
            if (!u->cp437 ||
                !u->glyph_font.draw(value, ax, Y + H - 6 - u->theme.mono_size, cell,
                                    u->theme.mono_size + 2,
                                    u->color(caret ? "background" : "muted"),
                                    u->color(caret ? "link" : hover == index ? "selection" : "surface")))
                text(u, caret ? "background" : "muted", glyph(value, u->cp437), ax, Y + H - 6);
        }
    }
    fl_pop_clip();
}
