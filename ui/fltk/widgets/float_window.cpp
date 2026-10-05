// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/widgets/float_window.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/panel.h"
#include "ui/fltk/util.h"
#include "ui/fltk/widgets/dock_surface.h"
#include <FL/Fl.H>

using zeal_ui::text;


FloatWindow::FloatWindow(dbg_ui_t *u, int panel, int x, int y, int w, int h)
    : Fl_Double_Window(x, y, w, h, zeal_ui::panel_name(panel)), ui(u), id(panel)
{
    end();
}

void FloatWindow::draw()
{
    Fl_Double_Window::draw();
    text(ui, "muted", "Drag here to dock · double-click to return", 8, ui->theme.row_height - 6, false);
}

int FloatWindow::handle(int event)
{
    Panel *video = ui->panels[zeal_ui::PANEL_VIDEO];
    if (id == zeal_ui::PANEL_VIDEO && video && video->captures_input() &&
        (event == FL_PUSH || event == FL_DRAG || event == FL_MOVE || event == FL_RELEASE ||
         event == FL_MOUSEWHEEL))
        return video->forward_input(event);
    if (event == FL_UNFOCUS)
        ui->release();
    if (event == FL_PUSH && Fl::event_y() < ui->theme.row_height) {
        if (Fl::event_clicks()) {
            ui->workspace.dock(id, -1, 0);
            ui->changed_layout();
            return 1;
        }
        ui->drag = id;
        return 1;
    }
    if (event == FL_DRAG && ui->drag == id) {
        ui->surface->redraw();
        return 1;
    }
    if (event == FL_RELEASE && ui->drag == id) {
        ui->drop(Fl::event_x_root(), Fl::event_y_root());
        ui->drag = -1;
        return 1;
    }
    return Fl_Double_Window::handle(event);
}
