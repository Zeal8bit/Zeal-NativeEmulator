// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/panels/video.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/input.h"
#include <FL/Fl.H>
#include <algorithm>

VideoCanvas::VideoCanvas(dbg_ui_t *u, int x, int y, int w, int h)
    : ImageCanvas(u, x, y, w, h, FIT)
{
}

void VideoCanvas::release_capture()
{
    if (captured && Fl::grab() == window())
        Fl::grab(nullptr);
    captured = false;
    fraction_x = fraction_y = 0;
}

void VideoCanvas::on_wheel(int delta, bool)
{
    ui->host.action(ui->host.debugger, UI_MOUSE_SPEED, -delta, 0);
    ui->message = "SNES mouse speed adjusted";
}

void VideoCanvas::on_click()
{
    mouse_x = Fl::event_x();
    mouse_y = Fl::event_y();
    if (Fl::event_button() == FL_MIDDLE_MOUSE) {
        captured = !captured;
        Fl::grab(captured ? window() : nullptr);
    }
    uint32_t buttons = (Fl::event_state(FL_BUTTON1) ? 1u : 0u) | (Fl::event_state(FL_BUTTON3) ? 2u : 0u);
    ui->host.mouse(ui->host.debugger, 0, 0, buttons);
}

void VideoCanvas::on_pointer(int)
{
    int row = ui->theme.row_height;
    double scale = std::max(.001, std::min(double(w()) / std::max(1u, image.width),
                                           double(h() - row) / std::max(1u, image.height)) *
                                          ui->scale);
    uint32_t buttons = (Fl::event_state(FL_BUTTON1) ? 1u : 0u) | (Fl::event_state(FL_BUTTON3) ? 2u : 0u);
    if (Fl::focus() == this) {
        fraction_x += (Fl::event_x() - mouse_x) / scale;
        fraction_y += (Fl::event_y() - mouse_y) / scale;
        int dx = int(fraction_x), dy = int(fraction_y);
        fraction_x -= dx;
        fraction_y -= dy;
        ui->host.mouse(ui->host.debugger, dx, dy, buttons);
    }
    mouse_x = Fl::event_x();
    mouse_y = Fl::event_y();
}

bool VideoCanvas::on_key(int event)
{
    if (!ui->passthrough && Fl::event_state(FL_COMMAND))
        return false;
    unsigned key = fltk_key_to_display(Fl::event_key());
    if (!key)
        return false;
    ui->host.key(ui->host.debugger, key, event != FL_KEYUP);
    return true;
}

void VideoCanvas::on_focus_lost()
{
    ui->release();
}

VideoPanel::VideoPanel(dbg_ui_t *u) : Panel(u, zeal_ui::PANEL_VIDEO)
{
    canvas = new VideoCanvas(u, 0, 0, 300, 206);
    finish();
}

bool VideoPanel::input_focused() const
{
    return Fl::focus() == canvas;
}

void VideoPanel::refresh()
{
    canvas->fetch(-1); // -1 is the live screen
    canvas->redraw();
}
