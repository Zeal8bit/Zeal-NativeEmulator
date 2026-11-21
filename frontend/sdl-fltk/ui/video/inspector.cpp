/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "video/inspector.h"
#include <cstdint>
#include <cstdio>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/machine.h"
#include "theme/theme.h"
#include "video/vram_image.h"

namespace zui::video {

using namespace zui::theme;

Inspector::Inspector(int X, int Y, int W, int H, VramImage* image)
    : Fl_Widget(X, Y, W, H), m_image(image)
{}

int Inspector::handle(int event)
{
    return event == FL_MOUSEWHEEL ? 1 : Fl_Widget::handle(event);
}

void Inspector::draw()
{
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    const int cell = m_image->pinned_cell >= 0 ? m_image->pinned_cell : m_image->hover_cell;
    draw_header(x(), y(), w(), "INSPECTOR", m_image->pinned_cell >= 0 ? "PINNED" : "");
    const fz_image_t& img = m_image->img;
    fl_font(FL_HELVETICA, 12);
    if (!m_image->valid) {
        fl_color(COLOR_DIM);
        fl_draw("The VRAM views are not available in bitmap modes.", x() + 12, y() + HEADER_H + 8,
                w() - 24, 60, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
        return;
    }
    if (cell < 0) {
        fl_color(COLOR_DIM);
        fl_draw("Hover a cell to inspect it.\nClick to pin, click again to unpin.", x() + 12, y() + HEADER_H + 8,
                w() - 24, 60, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
        return;
    }

    /* Magnified cell, including its grid border */
    const int cw = img.cell_width + 2;
    const int ch = img.cell_height + 2;
    const int cx = cell % img.columns;
    const int cy = cell / img.columns;
    const int sx0 = cx * (img.cell_width + 1);
    const int sy0 = cy * (img.cell_height + 1);
    int scale = (w() - 24) / cw;
    if (scale * ch > 200) scale = 200 / ch;
    if (scale < 1) scale = 1;
    const int bx = x() + (w() - cw * scale) / 2;
    const int by = y() + HEADER_H + 12;
    for (int yy = 0; yy < ch; yy++) {
        for (int xx = 0; xx < cw; xx++) {
            const int sx = sx0 + xx, sy = sy0 + yy;
            if (sx >= img.width || sy >= img.height) continue;
            const uint8_t* p = img.pixels + ((size_t) sy * img.width + sx) * 3;
            fl_color(fl_rgb_color(p[0], p[1], p[2]));
            fl_rectf(bx + xx * scale, by + yy * scale, scale, scale);
        }
    }
    fl_color(COLOR_BORDER);
    fl_rect(bx - 1, by - 1, cw * scale + 2, ch * scale + 2);

    /* Details */
    int ty = by + ch * scale + 24;
    char line[64];
    auto row = [&](const char* label, const char* value, Fl_Color c) {
        fl_font(FL_HELVETICA, 11);
        fl_color(COLOR_DIM);
        fl_draw(label, x() + 12, ty);
        fl_font(DATA_FONT, DATA_SIZE);
        fl_color(c);
        fl_draw(value, x() + 90, ty);
        ty += data_line_height() + 2;
    };
    switch (m_image->view) {
        case DBG_TILEMAP_LAYER0:
        case DBG_TILEMAP_LAYER1:
            snprintf(line, sizeof(line), "%d, %d", cx, cy);
            row("CELL", line, COLOR_TEXT);
            snprintf(line, sizeof(line), "%d", cell);
            row("INDEX", line, COLOR_TEXT);
            snprintf(line, sizeof(line), config.debugger.hex_upper ? "$%04X" : "$%04x", cell);
            row("OFFSET", line, COLOR_CYAN);
            break;
        case DBG_TILESET:
            snprintf(line, sizeof(line), config.debugger.hex_upper ? "%d ($%03X)" : "%d ($%03x)", cell, cell);
            row("TILE", line, COLOR_TEXT);
            break;
        case DBG_PALETTE: {
            const int px = sx0 + 1 + img.cell_width / 2;
            const int py = sy0 + 1 + img.cell_height / 2;
            const uint8_t* p = img.pixels + ((size_t) py * img.width + px) * 3;
            snprintf(line, sizeof(line), "%d ($%02X)", cell, cell);
            row("COLOR", line, COLOR_TEXT);
            snprintf(line, sizeof(line), "#%02X%02X%02X", p[0], p[1], p[2]);
            row("RGB", line, COLOR_CYAN);
            const unsigned rgb565 = ((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3);
            snprintf(line, sizeof(line), "$%04X", rgb565);
            row("RGB565", line, COLOR_TEXT);
            break;
        }
        case DBG_FONT:
            snprintf(line, sizeof(line), "%d ($%02X)", cell, cell);
            row("CHAR", line, COLOR_TEXT);
            if (cell >= 32 && cell < 127) {
                snprintf(line, sizeof(line), "'%c'", cell);
                row("ASCII", line, COLOR_AMBER);
            }
            break;
        default:
            break;
    }
}

} // namespace zui::video
