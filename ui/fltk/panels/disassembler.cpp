// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/panels/disassembler.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include "ui/fltk/widgets/text_input.h"
#include <FL/Fl.H>
#include <algorithm>

using zeal_ui::address;
using zeal_ui::hex;
using zeal_ui::mono_chars;

DisassemblerPanel::DisassemblerPanel(dbg_ui_t *u) : Panel(u, zeal_ui::PANEL_DISASSEMBLER)
{
    int row = u->theme.row_height;
    controls = new ControlRow(row);
    TextInput *loc = controls->add_location("0000", "Hex address or symbol; Enter to disassemble there");
    loc->callback(
        [](Fl_Widget *, void *data) {
            DisassemblerPanel *self = (DisassemblerPanel *)data;
            bool valid;
            uint32_t addr = address(self->ui, self->controls->location()->value(), &valid);
            if (!valid) {
                self->ui->message = "Unknown address or symbol";
                return;
            }
            self->start = addr;
            self->ui->last_refresh = 0;
            self->redraw();
        },
        this);
    controls->end();
    fixed(controls, row);
    table = new ListTable(this, u, 0, 0, 300, 206);
    finish();
}

void DisassemblerPanel::resize(int x, int y, int w, int h)
{
    int row = ui->theme.row_height, pad = ui->theme.spacing;
    controls->layout(row, pad, ui->ui_font, ui->theme.font_size);
    fixed(controls, row);
    Panel::resize(x, y, w, h);
}

const char *DisassemblerPanel::column_header(int column) const
{
    static const char *headers[] = {"", "Address", "Instruction", "Label"};
    return column < 4 ? headers[column] : "";
}

void DisassemblerPanel::column_widths(int *out, int table_width) const
{
    int fill = std::max(0, table_width - 24);
    // The instruction column is the one that matters, so it takes the slack and the
    // narrow label column stays visible; the label is usually empty anyway.
    out[0] = mono_chars(ui, 2);
    out[1] = mono_chars(ui, 6);
    out[3] = mono_chars(ui, 8);
    out[2] = std::max(mono_chars(ui, 12), fill - out[0] - out[1] - out[3]);
}

uint32_t DisassemblerPanel::row_address(int row) const
{
    return row >= 0 && row < int(instructions.size()) ? instructions[row].first : 0;
}

const char *DisassemblerPanel::row_background(int row) const
{
    if (row < 0 || row >= int(instructions.size()))
        return nullptr;
    return instructions[row].first == ui->snapshot.pc ? "current" : nullptr;
}

std::string DisassemblerPanel::cell_text(int row, int column, const char *&role) const
{
    if (row < 0 || row >= int(instructions.size()))
        return "";
    auto &[addr, in] = instructions[row];
    bool pc = addr == ui->snapshot.pc;
    if (column == 0) {
        role = "breakpoint";
        return debugger_is_breakpoint_set(ui->host.debugger, addr) ? "●" : "";
    }
    if (column == 1) {
        role = pc ? "link" : "text";
        return hex(addr, 4, ui->upper);
    }
    return column == 2 ? std::string(in.instruction) : std::string(in.label);
}

int DisassemblerPanel::action_id(int index) const
{
    static const int ids[] = {ACT_TOGGLE_BREAK, ACT_GOTO, ACT_COPY};
    return ids[index];
}

const char *DisassemblerPanel::action_label(int index) const
{
    static const char *labels[] = {"Toggle breakpoint", "Go to memory", "Copy address"};
    return labels[index];
}

void DisassemblerPanel::run_action(int action, int row)
{
    if (action != ACT_TOGGLE_BREAK || row < 0 || row >= int(instructions.size()))
        return;
    debugger_toggle_breakpoint(ui->host.debugger, instructions[row].first);
    ui->last_refresh = 0;
}

void DisassemblerPanel::refresh()
{
    if (shown_pc != ui->snapshot.pc && ui->snapshot.paused) {
        shown_pc = ui->snapshot.pc;
        start = shown_pc;
        table->reveal(0);
        if (Fl::focus() != controls->location())
            controls->location()->value(hex(start, 4).c_str());
    }
    instructions.clear();
    unsigned addr = start;
    for (int j = 0; j < 128; j++) {
        dbg_instr_t in{};
        int n = debugger_disassemble_address(ui->host.debugger, addr, &in);
        if (n <= 0)
            break;
        instructions.push_back({addr, in});
        addr = (addr + n) & 65535;
    }
    table->sync();
}
