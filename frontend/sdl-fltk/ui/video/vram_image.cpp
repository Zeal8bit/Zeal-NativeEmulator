/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "video/vram_image.h"
#include <cstddef>
#include <cstdint>
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/machine.h"
#include "theme/theme.h"
#include "video/inspector.h"

namespace zui::video {

using namespace zui::theme;

VramImage::VramImage(int X, int Y, int W, int H)
    : Fl_Widget(X, Y, W, H)
{}

void VramImage::update(dbg_vram_t v, int z)
{
    if (v != view) {
        hover_cell = pinned_cell = -1;
    }
    view = v;
    zoom = z;
    valid = fz_video_mode() != FZ_VIDEO_BITMAP && fz_vram_render(v, &img);
    if (!valid) {
        size(420, 60);
        redraw();
        return;
    }
    const int W = img.width * zoom;
    const int H = img.height * zoom;
    m_scaled.resize((size_t) W * H * 3);
    for (int yy = 0; yy < H; yy++) {
        const uint8_t* src = img.pixels + (size_t) (yy / zoom) * img.width * 3;
        uint8_t* dst = &m_scaled[(size_t) yy * W * 3];
        for (int xx = 0; xx < W; xx++) {
            const uint8_t* p = src + (xx / zoom) * 3;
            *dst++ = p[0];
            *dst++ = p[1];
            *dst++ = p[2];
        }
    }
    if (w() != W || h() != H) {
        size(W, H);
        if (parent()) parent()->redraw();
    }
    redraw();
}

int VramImage::cell_count() const
{
    if (!valid) return 0;
    return img.columns * (img.height / (img.cell_height + 1));
}

int VramImage::cell_at(int ex, int ey) const
{
    if (!valid) return -1;
    const int px = (ex - x()) / zoom;
    const int py = (ey - y()) / zoom;
    const int cx = px / (img.cell_width + 1);
    const int cy = py / (img.cell_height + 1);
    if (px < 0 || py < 0 || cx >= img.columns) return -1;
    const int cell = cy * img.columns + cx;
    return cell < cell_count() ? cell : -1;
}

void VramImage::draw()
{
    if (!valid) {
        fl_color(COLOR_BG);
        fl_rectf(x(), y(), w(), h());
        fl_color(COLOR_DIM);
        fl_font(FL_HELVETICA, 12);
        fl_draw("Not available in bitmap mode", x() + 12, y() + 24);
        return;
    }
    fl_draw_image(m_scaled.data(), x(), y(), w(), h(), 3);
    for (int sel : { hover_cell, pinned_cell }) {
        if (sel < 0) continue;
        const int cx = sel % img.columns;
        const int cy = sel / img.columns;
        fl_color(sel == pinned_cell ? COLOR_AMBER : COLOR_CYAN);
        fl_rect(x() + cx * (img.cell_width + 1) * zoom, y() + cy * (img.cell_height + 1) * zoom,
                (img.cell_width + 2) * zoom, (img.cell_height + 2) * zoom);
    }
}

int VramImage::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            hover_cell = -1;
            redraw();
            if (inspector) inspector->redraw();
            return 1;
        case FL_MOVE: {
            const int cell = cell_at(Fl::event_x(), Fl::event_y());
            if (cell != hover_cell) {
                hover_cell = cell;
                redraw();
                if (inspector) inspector->redraw();
            }
            return 1;
        }
        case FL_PUSH: {
            const int cell = cell_at(Fl::event_x(), Fl::event_y());
            pinned_cell = (cell == pinned_cell) ? -1 : cell;
            redraw();
            if (inspector) inspector->redraw();
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

} // namespace zui::video
