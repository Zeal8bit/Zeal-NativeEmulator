// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/panels/breakpoints.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include "ui/fltk/widgets/text_input.h"
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/fl_ask.H>
#include <algorithm>

using zeal_ui::address;
using zeal_ui::hex;
using zeal_ui::mono_chars;

BreakpointsPanel::BreakpointsPanel(dbg_ui_t *u) : Panel(u, zeal_ui::PANEL_BREAKPOINTS)
{
    int row = u->theme.row_height, pad = u->theme.spacing;
    // The address/range row is its own Fl_Flex so the input takes the slack and the
    // choice keeps a width derived from its widest item, instead of magic pixels.
    controls = new ControlRow(row);
    TextInput *loc = controls->add_location("0000", "Hex address or symbol; Enter to add an entry");
    controls->add_choice("Breakpoint|Watch Read|Watch Write|Watch R+W");
    loc->callback([](Fl_Widget *, void *data) { ((BreakpointsPanel *)data)->add_from_controls(); }, this);
    controls->end();
    fixed(controls, row);
    table = new ListTable(this, u, 0, 0, 300, 206);
    begin(); // Fl_Table leaves Fl_Group::current() on its inner scroll widget
    actions = new Fl_Flex(0, 0, 300, row, Fl_Flex::HORIZONTAL);
    actions->gap(pad);
    actions->begin();
    static const char *labels[] = {"Add...", "Edit...", "Remove"};
    for (int j = 0; j < 3; j++)
        action_buttons[j] = new Fl_Button(0, 0, 0, row, labels[j]);
    actions->end();
    fixed(actions, row);
    action_buttons[0]->callback([](Fl_Widget *, void *d) { ((BreakpointsPanel *)d)->add_entry(); }, this);
    action_buttons[1]->callback([](Fl_Widget *, void *d) { ((BreakpointsPanel *)d)->edit_entry(); }, this);
    action_buttons[2]->callback(
        [](Fl_Widget *, void *d) { ((BreakpointsPanel *)d)->table->remove_selected(); }, this);
    finish();
}

void BreakpointsPanel::resize(int x, int y, int w, int h)
{
    int row = ui->theme.row_height, pad = ui->theme.spacing;
    controls->layout(row, pad, ui->ui_font, ui->theme.font_size);
    fixed(controls, row);
    fixed(actions, row);
    Panel::resize(x, y, w, h);
}

int BreakpointsPanel::row_count() const
{
    return int(breakpoints.size() + watchpoints.size());
}

const char *BreakpointsPanel::column_header(int column) const
{
    static const char *headers[] = {"Type", "Address", "Symbol"};
    return column < 3 ? headers[column] : "";
}

void BreakpointsPanel::column_widths(int *out, int table_width) const
{
    int fill = std::max(0, table_width - 24); // slack for the frame and vertical scrollbar
    out[0] = mono_chars(ui, 11);
    out[1] = mono_chars(ui, 7);
    out[2] = std::max(mono_chars(ui, 10), fill - out[0] - out[1]);
}

uint32_t BreakpointsPanel::row_address(int row) const
{
    if (row < 0)
        return 0;
    int nb = int(breakpoints.size());
    if (row < nb)
        return breakpoints[row];
    return row - nb < int(watchpoints.size()) ? watchpoints[row - nb].addr : 0;
}

std::string BreakpointsPanel::cell_text(int row, int column, const char *&role) const
{
    int nb = int(breakpoints.size());
    uint32_t addr;
    if (row < nb) {
        addr = breakpoints[row];
        if (column == 0) {
            role = "breakpoint";
            return "Breakpoint";
        }
    } else {
        watchpoint_t wp = watchpoints[row - nb];
        addr = wp.addr;
        if (column == 0) {
            role = "warning";
            return wp.type == WATCHPOINT_RW     ? "Watch R+W"
                   : wp.type == WATCHPOINT_READ ? "Watch R"
                                                : "Watch W";
        }
    }
    if (column == 1)
        return hex(addr, 4, ui->upper);
    const char *symbol = debugger_get_symbol(ui->host.debugger, addr);
    return symbol ? symbol : "";
}

int BreakpointsPanel::action_id(int index) const
{
    static const int ids[] = {ACT_REMOVE, ACT_GOTO, ACT_COPY};
    return ids[index];
}

const char *BreakpointsPanel::action_label(int index) const
{
    static const char *labels[] = {"Remove", "Go to memory", "Copy address"};
    return labels[index];
}

void BreakpointsPanel::run_action(int action, int row)
{
    if (action != ACT_REMOVE)
        return;
    dbg_t *d = ui->host.debugger;
    int nb = int(breakpoints.size());
    if (row < nb)
        debugger_clear_breakpoint(d, breakpoints[row]);
    else if (row - nb < int(watchpoints.size()))
        debugger_remove_watchpoint(d, watchpoints[row - nb].addr);
    ui->last_refresh = 0;
}

void BreakpointsPanel::selection_changed(int row, int rows)
{
    bool has = row >= 0 && rows > 0;
    action_buttons[0]->activate(); // Add is always available
    if (has) {
        action_buttons[1]->activate();
        action_buttons[2]->activate();
    } else {
        action_buttons[1]->deactivate();
        action_buttons[2]->deactivate();
    }
}

void BreakpointsPanel::add_at(uint32_t addr)
{
    int type = controls->choice()->value();
    if (type) {
        debugger_add_watchpoint(ui->host.debugger, {addr, (watchpoint_type_t)type});
        ui->message = "Added watchpoint at $" + hex(addr, 4);
    } else {
        debugger_set_breakpoint(ui->host.debugger, addr);
        ui->message = "Added breakpoint at $" + hex(addr, 4);
    }
    ui->last_refresh = 0;
    redraw();
}

void BreakpointsPanel::add_from_controls()
{
    bool valid;
    uint32_t addr = address(ui, controls->location()->value(), &valid);
    if (!valid) {
        ui->message = "Unknown address or symbol";
        return;
    }
    add_at(addr);
}

void BreakpointsPanel::add_entry()
{
    std::string current = controls->location()->value();
    const char *entered = fl_input("Address or symbol for the new entry:", current.c_str());
    if (!entered)
        return;
    bool valid = false;
    uint32_t addr = address(ui, entered, &valid);
    if (!valid) {
        ui->message = "Unknown address or symbol";
        return;
    }
    add_at(addr);
}

void BreakpointsPanel::edit_entry()
{
    int row = table->selected_row();
    if (row < 0 || row >= row_count())
        return;
    uint32_t old_addr = row_address(row);
    const char *symbol = debugger_get_symbol(ui->host.debugger, old_addr);
    std::string current = symbol ? symbol : hex(old_addr, 4);
    const char *entered = fl_input("New address or symbol for the selected entry:", current.c_str());
    if (!entered)
        return;
    bool valid = false;
    uint32_t addr = address(ui, entered, &valid);
    if (!valid) {
        ui->message = "Unknown address or symbol";
        return;
    }
    if (addr == old_addr)
        return;
    // There is no "move" call, so the entry is re-created with the same kind.
    int nb = int(breakpoints.size());
    if (row < nb) {
        debugger_clear_breakpoint(ui->host.debugger, old_addr);
        debugger_set_breakpoint(ui->host.debugger, addr);
    } else {
        watchpoint_t wp = watchpoints[row - nb];
        debugger_remove_watchpoint(ui->host.debugger, old_addr);
        wp.addr = addr;
        debugger_add_watchpoint(ui->host.debugger, wp);
    }
    ui->message = "Moved entry to $" + hex(addr, 4);
    ui->last_refresh = 0;
    redraw();
}

void BreakpointsPanel::refresh()
{
    dbg_t *d = ui->host.debugger;
    hwaddr b[DBG_MAX_POINTS];
    int n = debugger_get_breakpoints(d, b, DBG_MAX_POINTS);
    breakpoints.assign(b, b + n);
    watchpoint_t w[DBG_MAX_POINTS];
    n = debugger_get_watchpoints(d, w, DBG_MAX_POINTS);
    watchpoints.assign(w, w + n);
    table->sync();
}
