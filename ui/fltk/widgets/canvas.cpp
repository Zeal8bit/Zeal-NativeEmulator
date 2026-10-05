// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/widgets/canvas.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/util.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using zeal_ui::text;

ImageCanvas::ImageCanvas(dbg_ui_t *u, int x, int y, int w, int h, Placement p)
    : Fl_Widget(x, y, w, h), ui(u), placement(p)
{
}

bool ImageCanvas::fetch(int view)
{
    dbg_image_info_t info{};
    dbg_status_t result = debugger_image_copy(ui->host.debugger, view, &info, nullptr, 0);
    if (result != DBG_CAPACITY)
        return false;
    pixels.resize(info.stride * info.height);
    if (debugger_image_copy(ui->host.debugger, view, &info, pixels.data(), pixels.size()) != DBG_OK)
        return false;
    image = info;
    return true;
}

int ImageCanvas::maximum_scroll(bool horizontal) const
{
    if (placement != SCALED)
        return 0;
    int extent = horizontal ? image.width : image.height;
    return std::max(0, int(extent) * ui->scale - (horizontal ? w() : h()));
}

void ImageCanvas::on_wheel(int delta, bool horizontal)
{
    int &offset = horizontal ? scroll_x : scroll_y;
    offset = std::clamp(offset + delta * 32, 0, maximum_scroll(horizontal));
    redraw();
}

void ImageCanvas::draw()
{
    int row = ui->theme.row_height, pad = ui->theme.spacing;
    fl_push_clip(x(), y(), w(), h());
    fl_color(ui->color("surface"));
    fl_rectf(x(), y(), w(), h());
    int baseline = y() + row - 6;
    if (!pixels.empty() && image.width) {
        int iw = image.width, ih = image.height;
        int dw, dh, ox, oy;
        if (placement == FIT) {
            double fit = std::min(double(w()) / iw, double(h() - row) / ih) * ui->scale;
            dw = std::max(1, int(iw * fit));
            dh = std::max(1, int(ih * fit));
            ox = x() + (w() - dw) / 2;
            oy = y() + (h() - row - dh) / 2;
        } else {
            dw = iw * ui->scale;
            dh = ih * ui->scale;
            ox = x() - scroll_x;
            oy = y() - scroll_y;
        }
        // Explicit nearest-neighbor scaling, independent of FLTK image filters.
        int left = std::max(x(), ox), top = std::max(y(), oy), right = std::min(x() + w(), ox + dw),
            bottom = std::min(y() + h() - row, oy + dh);
        if (right > left && bottom > top) {
            std::vector<unsigned char> scaled((right - left) * (bottom - top) * 3);
            for (int yy = top; yy < bottom; yy++)
                for (int xx = left; xx < right; xx++) {
                    unsigned src = ((yy - oy) * ih / dh) * image.stride + ((xx - ox) * iw / dw) * 4;
                    unsigned char *dst = &scaled[((yy - top) * (right - left) + xx - left) * 3];
                    std::memcpy(dst, &pixels[src], 3);
                }
            fl_draw_image(scaled.data(), left, top, right - left, bottom - top, 3);
        }
        decorate(row, pad);
        text(ui, "muted", footer(), x() + pad, y() + h() - 5, false);
    } else
        text(ui, "muted", "Waiting for frame", x() + pad, baseline, false);
    fl_pop_clip();
}

int ImageCanvas::handle(int event)
{
    if (event == FL_UNFOCUS || event == FL_HIDE) {
        on_focus_lost();
        return 1;
    }
    if (event == FL_FOCUS)
        return 1;
    if (event == FL_PUSH) {
        take_focus();
        on_click();
        ui->last_refresh = 0;
        return 1;
    }
    if (event == FL_MOUSEWHEEL) {
        on_wheel(Fl::event_dy(), Fl::event_state(FL_SHIFT) != 0);
        return 1;
    }
    if (event == FL_MOVE || event == FL_DRAG || event == FL_RELEASE) {
        if (placement == SCALED) {
            int px = (Fl::event_x() - x() + scroll_x) / std::max(1, ui->scale);
            int py = (Fl::event_y() - y() + scroll_y) / std::max(1, ui->scale);
            hover_at(px, py);
        }
        on_pointer(event);
        return 1;
    }
    if (event == FL_KEYDOWN || event == FL_KEYUP || event == FL_SHORTCUT) {
        if (on_key(event))
            return 1;
    }
    return Fl_Widget::handle(event);
}
