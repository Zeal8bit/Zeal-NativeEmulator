// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/panels/cpu.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include "ui/fltk/widgets/text_input.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Grid.H>
#include <FL/Fl_Input.H>
#include <FL/fl_draw.H>
#include <algorithm>

using zeal_ui::address;
using zeal_ui::text;
using zeal_ui::hex;

CpuPanel::CpuPanel(dbg_ui_t *u) : Panel(u, zeal_ui::PANEL_CPU)
{
    static const char *labels[] = {"AF", "BC",  "DE",  "HL",  "PC",  "SP", "IX",
                                   "IY", "AF'", "BC'", "DE'", "HL'", "IR", "F"};
    int row = u->theme.row_height, pad = u->theme.spacing;
    grid = new Fl_Grid(0, 0, 300, 180);
    grid->layout(7, 4, 0, pad);
    grid->begin();
    // Fl_Grid defaults every row/column weight to 50, so the label columns must be
    // zeroed explicitly: only the input columns should absorb extra space, and rows
    // should keep exactly the theme row height.
    grid->col_weight(0, 0);
    grid->col_weight(1, 1);
    grid->col_weight(2, 0);
    grid->col_weight(3, 1);
    for (int r = 0; r < 7; r++)
        grid->row_weight(r, 0);
    for (int j = 0; j < 14; j++) {
        Fl_Box *lb = new Fl_Box(0, 0, 0, row, labels[j]);
        lb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        register_labels[j] = lb;
        TextInput *in = new TextInput(0, 0, 0, row);
        registers[j] = in;
        grid->widget(lb, j / 2, (j % 2) * 2, FL_GRID_FILL);
        grid->widget(in, j / 2, (j % 2) * 2 + 1, FL_GRID_FILL);
        if (j == 13) {
            in->readonly(1);
            continue;
        }
        in->maximum_size(4);
        in->when(FL_WHEN_ENTER_KEY_ALWAYS);
        in->callback([](Fl_Widget *w, void *data) { ((CpuPanel *)data)->apply_register(w); }, this);
        in->tooltip(
            "Edit hex value, Enter to apply while paused; double-click label navigates memory");
    }
    grid->end();
    flags = new FlagsBar(u, 0, 0, 300, 30);
    fixed(flags, 30); // the flags line stays put below the register grid
    finish();
}

void CpuPanel::apply_register(Fl_Widget *field)
{
    if (!ui->snapshot.paused)
        return;
    bool valid;
    uint32_t v = address(ui, ((Fl_Input *)field)->value(), &valid);
    if (!valid)
        return;
    debugger_get_registers(ui->host.debugger, &regs);
    uint16_t *vals[] = {&regs.af,  &regs.bc,  &regs.de,  &regs.hl,  &regs.pc,  &regs.sp, &regs.ix,
                        &regs.iy, &regs.af_, &regs.bc_, &regs.de_, &regs.hl_, &regs.ir};
    for (int k = 0; k < 13; k++)
        if (registers[k] == field)
            *vals[k] = v;
    debugger_set_registers(ui->host.debugger, &regs);
    field->clear_changed();
    ui->last_refresh = 0;
}

void CpuPanel::resize(int x, int y, int w, int h)
{
    int row = ui->theme.row_height, pad = ui->theme.spacing;
    if (grid) {
        fl_font(ui->ui_font, ui->theme.font_size);
        int label_w = 0;
        for (int j = 0; j < 14; j++)
            label_w = std::max(label_w, int(fl_width(register_labels[j]->label())));
        int input_w = int(fl_width("0000")) + 2 * pad;
        for (int r = 0; r < 7; r++)
            grid->row_height(r, row);
        grid->col_width(0, label_w + 2 * pad);
        grid->col_width(2, label_w + 2 * pad);
        grid->col_width(1, input_w);
        grid->col_width(3, input_w);
    }
    Panel::resize(x, y, w, h);
}

int CpuPanel::handle(int event)
{
    if (event == FL_PUSH && (Fl::event_button() == FL_RIGHT_MOUSE || Fl::event_clicks())) {
        for (int i = 0; i < 13; i++) {
            Fl_Box *lb = register_labels[i];
            if (Fl::event_y() >= lb->y() && Fl::event_y() < lb->y() + lb->h() &&
                Fl::event_x() >= lb->x() && Fl::event_x() < lb->x() + lb->w()) {
                bool valid;
                uint32_t addr = address(ui, registers[i]->value(), &valid);
                if (valid)
                    ui->navigate(addr);
                return 1;
            }
        }
    }
    return Fl_Group::handle(event);
}

void CpuPanel::refresh()
{
    dbg_t *d = ui->host.debugger;
    debugger_get_registers(d, &regs);
    const uint16_t values[] = {regs.af, regs.bc,  regs.de,  regs.hl,  regs.pc,  regs.sp, regs.ix,
                               regs.iy, regs.af_, regs.bc_, regs.de_, regs.hl_, regs.ir, regs.f};
    for (int i = 0; i < 14; i++) {
        registers[i]->readonly(!ui->snapshot.paused || i == 13);
        if (Fl::focus() != registers[i] && !registers[i]->changed())
            registers[i]->value(hex(values[i], i == 13 ? 2 : 4, ui->upper).c_str());
    }
    flags->flags = regs.f;
    flags->redraw();
}

void FlagsBar::draw()
{
    static const char *flag_names = "SZ5H3PNC";
    std::string s;
    for (int i = 0; i < 8; i++) {
        s += flags & (128 >> i) ? flag_names[i] : '.';
        s += ' ';
    }
    fl_push_clip(x(), y(), w(), h());
    fl_color(ui->color("surface"));
    fl_rectf(x(), y(), w(), h());
    text(ui, "muted", s, x() + ui->theme.spacing, y() + ui->theme.row_height - 6);
    fl_pop_clip();
}
