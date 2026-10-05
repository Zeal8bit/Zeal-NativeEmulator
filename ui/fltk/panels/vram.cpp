// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/panels/vram.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include <FL/Fl_Choice.H>
#include <cstring>
#include <string>
#include <vector>

using zeal_ui::hex;

VramCanvas::VramCanvas(VramPanel *p, dbg_ui_t *u, int x, int y, int w, int h)
    : ImageCanvas(u, x, y, w, h, SCALED), panel(p)
{
}

std::string VramCanvas::footer() const
{
    if (hover < 0)
        return "Wheel scroll  Shift horizontal";
    dbg_vram_info_t info{};
    if (debugger_vram_info(ui->host.debugger, panel->selected_view(), hover, &info) != DBG_OK)
        return "Wheel scroll  Shift horizontal";
    std::string detail = "Cell " + std::to_string(info.index) + "  value $" +
                         hex(info.value, info.value > 255 ? 4 : 2) + "  attr $" + hex(info.attributes, 2);
    if (panel->selected_view() == DBG_PALETTE)
        detail += "  RGB #" + hex(info.palette_rgb, 6);
    return detail;
}

void VramCanvas::decorate(int row, int)
{
    if (hover < 0)
        return;
    dbg_vram_info_t info{};
    if (debugger_vram_info(ui->host.debugger, panel->selected_view(), hover, &info) != DBG_OK)
        return;
    int iw = image.width, ih = image.height;
    int cw = info.cell_width, ch = info.cell_height,
        cx = (info.index % info.columns) * cw + 1, cy = (info.index / info.columns) * ch + 1;
    int zoom = 4, pw = (cw - 1) * zoom, ph = (ch - 1) * zoom;
    if (cx + cw <= iw && cy + ch <= ih && w() > pw + 16 && h() > ph + row + 16) {
        std::vector<unsigned char> preview(pw * ph * 3);
        for (int yy = 0; yy < ph; yy++)
            for (int xx = 0; xx < pw; xx++)
                std::memcpy(&preview[(yy * pw + xx) * 3],
                            &pixels[(cy + yy / zoom) * image.stride + (cx + xx / zoom) * 4], 3);
        fl_draw_image(preview.data(), x() + w() - pw - 8, y() + 8, pw, ph, 3);
    }
}

void VramCanvas::hover_at(int px, int py)
{
    const dbg_vram_info_t &v = panel->vram;
    hover = v.cell_width && v.cell_height && px >= 0 && py >= 0
                ? int((py / v.cell_height) * v.columns + px / v.cell_width)
                : -1;
    if (px >= int(v.columns * v.cell_width) || py >= int(v.rows * v.cell_height))
        hover = -1;
    redraw();
}

VramPanel::VramPanel(dbg_ui_t *u) : Panel(u, zeal_ui::PANEL_VRAM)
{
    int row = u->theme.row_height;
    controls = new ControlRow(row);
    Fl_Choice *view = controls->add_choice("Layer 0|Layer 1|Tileset|Palette|Font");
    view->callback(
        [](Fl_Widget *, void *data) {
            VramPanel *self = (VramPanel *)data;
            self->canvas->scroll_x = self->canvas->scroll_y = 0;
            self->ui->last_refresh = 0;
            self->redraw();
        },
        this);
    controls->end();
    fixed(controls, row);
    canvas = new VramCanvas(this, u, 0, 0, 300, 206);
    finish();
}

int VramPanel::selected_view() const
{
    return controls->choice()->value();
}

void VramPanel::select_view(int view)
{
    controls->choice()->value(view);
    canvas->scroll_x = canvas->scroll_y = 0;
    ui->last_refresh = 0;
}

void VramPanel::resize(int x, int y, int w, int h)
{
    int row = ui->theme.row_height, pad = ui->theme.spacing;
    controls->layout(row, pad, ui->ui_font, ui->theme.font_size);
    fixed(controls, row);
    Panel::resize(x, y, w, h);
}

void VramPanel::refresh()
{
    int view = controls->choice()->value();
    debugger_vram_info(ui->host.debugger, view, 0, &vram);
    canvas->fetch(view);
    canvas->redraw();
}
