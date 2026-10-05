// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/panels/memory.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include "ui/fltk/widgets/text_input.h"
#include <FL/Fl_Choice.H>
#include <algorithm>

using zeal_ui::address;

MemoryPanel::MemoryPanel(dbg_ui_t *u) : Panel(u, zeal_ui::PANEL_MEMORY)
{
    int row = u->theme.row_height;
    controls = new ControlRow(row);
    TextInput *loc = controls->add_location("0000", "Hex address or symbol; Enter to navigate");
    Fl_Choice *range_choice = controls->add_choice("256 bytes|1 KiB|4 KiB|16 KiB|64 KiB");
    loc->callback(
        [](Fl_Widget *, void *data) {
            MemoryPanel *self = (MemoryPanel *)data;
            bool valid;
            uint32_t addr = address(self->ui, self->controls->location()->value(), &valid);
            if (!valid) {
                self->ui->message = "Unknown address or symbol";
                return;
            }
            self->start = addr;
            self->table->reset_scroll();
            self->ui->last_refresh = 0;
            self->redraw();
        },
        this);
    range_choice->callback(
        [](Fl_Widget *, void *data) {
            static const uint32_t ranges[] = {256, 1024, 4096, 16384, 65536};
            MemoryPanel *self = (MemoryPanel *)data;
            self->range = ranges[self->controls->choice()->value()];
            self->ui->last_refresh = 0;
            self->redraw();
        },
        this);
    controls->end();
    fixed(controls, row);
    table = new MemoryTable(this, u, 0, 0, 300, 206);
    finish();
}

void MemoryPanel::resize(int x, int y, int w, int h)
{
    int row = ui->theme.row_height, pad = ui->theme.spacing;
    controls->layout(row, pad, ui->ui_font, ui->theme.font_size);
    fixed(controls, row);
    Panel::resize(x, y, w, h);
}

void MemoryPanel::go_to_address(uint32_t addr)
{
    start = addr;
    table->reset_scroll();
    controls->location()->value(zeal_ui::hex(addr, 4).c_str());
    ui->last_refresh = 0;
    redraw();
}

void MemoryPanel::refresh()
{
    unsigned size = std::min(range, 65536 - start);
    memory.resize(size);
    debugger_memory_read(ui->host.debugger, DBG_VIRTUAL, start, memory.data(), size);
    if (table->refresh_view(size, start, ui->upper, ui->cp437))
        table->redraw();
}
