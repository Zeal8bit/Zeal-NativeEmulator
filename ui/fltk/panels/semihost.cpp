// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/panels/semihost.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include <string>

using zeal_ui::mono_chars;

SemihostPanel::SemihostPanel(dbg_ui_t *u) : Panel(u, zeal_ui::PANEL_SEMIHOST)
{
    table = new ListTable(this, u, 0, 0, 300, 206);
    finish();
}

void SemihostPanel::refresh()
{
    debugger_counters(ui->host.debugger, counters);
    table->sync();
}

const char *SemihostPanel::column_header(int column) const
{
    static const char *headers[] = {"ID", "State", "Last us", "Min", "Max", "Avg", "n"};
    return column < 7 ? headers[column] : "";
}

void SemihostPanel::column_widths(int *out, int table_width) const
{
    (void)table_width;
    out[0] = mono_chars(ui, 3);
    for (int c = 1; c < 6; c++)
        out[c] = mono_chars(ui, 8);
    out[6] = mono_chars(ui, 6);
}

std::string SemihostPanel::cell_text(int row, int column, const char *&role) const
{
    if (row < 0 || row >= 8)
        return "";
    const dbg_counter_t &c = counters[row];
    switch (column) {
    case 0:
        if (c.break_on_update)
            role = "breakpoint";
        return std::to_string(row) + (c.break_on_update ? " *" : "");
    case 1: return c.running ? "running" : "stopped";
    case 2: return std::to_string(c.last_us);
    case 3: return std::to_string(c.minimum_us);
    case 4: return std::to_string(c.maximum_us);
    case 5: return std::to_string(c.samples ? c.total_us / c.samples : 0);
    default: return std::to_string(c.samples);
    }
}

void SemihostPanel::run_action(int action, int row)
{
    if (action != ACT_TOGGLE_COUNTER || row < 0 || row >= 8)
        return;
    debugger_counter_break(ui->host.debugger, row, !counters[row].break_on_update);
    ui->last_refresh = 0;
}
