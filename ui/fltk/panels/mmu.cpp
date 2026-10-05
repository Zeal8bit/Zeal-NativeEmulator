// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/panels/mmu.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include <algorithm>

using zeal_ui::hex;
using zeal_ui::mono_chars;

MmuPanel::MmuPanel(dbg_ui_t *u) : Panel(u, zeal_ui::PANEL_MMU)
{
    table = new ListTable(this, u, 0, 0, 300, 206);
    finish();
}

void MmuPanel::refresh()
{
    debugger_mappings(ui->host.debugger, mappings);
    table->sync();
}

const char *MmuPanel::column_header(int column) const
{
    static const char *headers[] = {"Virtual", "Page", "Physical", "Device"};
    return column < 4 ? headers[column] : "";
}

void MmuPanel::column_widths(int *out, int table_width) const
{
    int fill = std::max(0, table_width - 24);
    out[0] = mono_chars(ui, 5);
    out[1] = mono_chars(ui, 3);
    out[2] = mono_chars(ui, 7);
    out[3] = std::max(mono_chars(ui, 5), fill - out[0] - out[1] - out[2]);
}

uint32_t MmuPanel::row_address(int row) const
{
    return row >= 0 && row < 4 ? mappings[row].virtual_address : 0;
}

std::string MmuPanel::cell_text(int row, int column, const char *&role) const
{
    if (row < 0 || row >= 4)
        return "";
    const dbg_mapping_t &m = mappings[row];
    switch (column) {
    case 0:
        role = "link";
        return hex(m.virtual_address, 4, ui->upper);
    case 1: return hex(m.page, 2, ui->upper);
    case 2: return hex(m.physical_address, 6, ui->upper);
    default: return m.device;
    }
}

int MmuPanel::action_id(int index) const
{
    static const int ids[] = {ACT_GOTO, ACT_COPY};
    return ids[index];
}

const char *MmuPanel::action_label(int index) const
{
    static const char *labels[] = {"Go to memory", "Copy address"};
    return labels[index];
}
