/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "video/sprite_inspector.h"
#include <cstdio>
#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/machine.h"
#include "theme/theme.h"
#include "video/sprite_data.h"

namespace zui::video {

using namespace zui::theme;

SpriteInspector::SpriteInspector(int X, int Y, int W, int H, SpriteData* data)
    : Fl_Widget(X, Y, W, H), m_data(data)
{}

void SpriteInspector::draw()
{
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    const int cur = m_data->current();
    char extra[40] = "";
    if (cur >= 0) {
        snprintf(extra, sizeof(extra), "#%d  16x%d", cur, m_data->height(cur));
    }
    draw_header(x(), y(), w(), "SPRITE", extra);

    /* Screen map: where every on-screen sprite sits */
    map_rect(m_mx, m_my, m_mw, m_mh);
    fl_color(COLOR_BG);
    fl_rectf(m_mx, m_my, m_mw, m_mh);
    fl_color(COLOR_BORDER);
    fl_rect(m_mx - 1, m_my - 1, m_mw + 2, m_mh + 2);
    fl_font(FL_HELVETICA, 10);
    fl_color(COLOR_DIM);
    char res[48];
    snprintf(res, sizeof(res), "%dx%d%s", m_data->screen_w, m_data->screen_h,
             m_data->selected >= 0 ? "   pinned (click again to release)" : "");
    fl_draw(res, m_mx + 3, m_my + m_mh - 4);
    fl_push_clip(m_mx, m_my, m_mw, m_mh);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = FZ_SPRITE_COUNT - 1; i >= 0; i--) {
            if (!m_data->on_screen(i)) {
                continue;
            }
            const bool is_cur = i == cur;
            if ((pass == 1) != is_cur) {
                continue;
            }
            int rx, ry, rw, rh;
            sprite_rect(i, rx, ry, rw, rh);
            fl_color(is_cur ? COLOR_AMBER : COLOR_CYAN);
            if (is_cur) {
                fl_rectf(rx, ry, rw, rh);
            } else {
                fl_rect(rx, ry, rw, rh);
            }
        }
    }
    fl_pop_clip();

    int ty = m_my + m_mh + 14;
    fl_font(FL_HELVETICA, 12);
    if (cur < 0) {
        fl_color(COLOR_DIM);
        fl_draw("Hover a sprite, or click one in the\nsheet or on the map to pin it.", x() + 12, ty,
                w() - 24, 40, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
        return;
    }

    /* Magnified sprite with its state flags beside it */
    const int scale = 4;
    compose_sprite(*m_data, cur, scale, false, m_buf);
    fl_draw_image(m_buf.data(), x() + 12, ty, FZ_SPRITE_W * scale, FZ_SPRITE_MAX_H * scale, 3);
    fl_color(COLOR_BORDER);
    fl_rect(x() + 11, ty - 1, FZ_SPRITE_W * scale + 2, FZ_SPRITE_MAX_H * scale + 2);

    const int lh = data_line_height();
    const int ax = x() + 12 + FZ_SPRITE_W * scale + 14;
    int ay = ty + 12;
    auto info = [&](const char* label, const char* value, Fl_Color c) {
        fl_font(FL_HELVETICA, 10);
        fl_color(COLOR_DIM);
        fl_draw(label, ax, ay);
        fl_font(DATA_FONT, DATA_SIZE);
        fl_color(c);
        fl_draw(value, ax + 48, ay);
        ay += lh + 2;
    };
    info("STATE", m_data->on_screen(cur) ? "on screen" : "off screen",
         m_data->on_screen(cur) ? COLOR_GREEN : COLOR_AMBER);
    char v[48];
    snprintf(v, sizeof(v), "%s%s", m_data->flip_x(cur) ? "X " : "",
                 m_data->flip_y(cur) ? "Y" : (m_data->flip_x(cur) ? "" : "-"));
    info("FLIP", v, COLOR_TEXT);
    info("PRIO", m_data->behind_fg(cur) ? "behind" : "front",
         m_data->behind_fg(cur) ? COLOR_DIM : COLOR_TEXT);
    info("BANK", m_data->info[cur].flags.bitmap.tileset_idx ? "1" : "0", COLOR_TEXT);

    /* Numbers: explicit HEX and DEC columns */
    const bool up = config.debugger.hex_upper;
    const int col_label = x() + 12;
    const int col_hex = col_label + 64;
    const int col_dec = col_hex + 78;
    int ry = ty + FZ_SPRITE_MAX_H * scale + 22;
    fl_font(FL_HELVETICA, 10);
    fl_color(COLOR_DIM);
    fl_draw("HEX", col_hex, ry);
    fl_draw("DEC", col_dec, ry);
    fl_color(COLOR_BORDER);
    fl_xyline(col_label, ry + 4, x() + w() - 12);
    ry += lh + 2;
    char hex[24], dec[24];
    auto num = [&](const char* label, const char* h, const char* d, Fl_Color c) {
        fl_font(FL_HELVETICA, 10);
        fl_color(COLOR_DIM);
        fl_draw(label, col_label, ry);
        fl_font(DATA_FONT, DATA_SIZE);
        fl_color(c);
        if (h) {
            fl_draw(h, col_hex, ry);
        }
        if (d) {
            fl_draw(d, col_dec, ry);
        }
        ry += lh + 2;
    };
    snprintf(hex, sizeof(hex), up ? "$%04X" : "$%04x", m_data->info[cur].x);
    snprintf(dec, sizeof(dec), "%u", m_data->info[cur].x);
    num("X", hex, dec, COLOR_TEXT);
    snprintf(hex, sizeof(hex), up ? "$%04X" : "$%04x", m_data->info[cur].y);
    snprintf(dec, sizeof(dec), "%u", m_data->info[cur].y);
    num("Y", hex, dec, COLOR_TEXT);
    snprintf(dec, sizeof(dec), "%d, %d", m_data->screen_x(cur), m_data->screen_y(cur));
    num("SCREEN", nullptr, dec, m_data->on_screen(cur) ? COLOR_GREEN : COLOR_AMBER);
    snprintf(hex, sizeof(hex), up ? "$%03X" : "$%03x", m_data->tile(cur));
    snprintf(dec, sizeof(dec), "%u", m_data->tile(cur));
    num("TILE", hex, dec, COLOR_CYAN);
    snprintf(hex, sizeof(hex), up ? "$%X" : "$%x", m_data->palette(cur));
    snprintf(dec, sizeof(dec), "%u", m_data->palette(cur));
    num("PALETTE", hex, dec, COLOR_TEXT);
    snprintf(hex, sizeof(hex), up ? "$%04X" : "$%04x", m_data->raw_flags(cur));
    num("FLAGS", hex, nullptr, COLOR_DIM);
    snprintf(hex, sizeof(hex), up ? "$%04X" : "$%04x", m_data->raw_extra(cur));
    num("EXTRA", hex, nullptr, COLOR_DIM);
}

int SpriteInspector::handle(int event)
{
    if (event == FL_MOUSEWHEEL) {
        return 1;
    }
    if (event == FL_PUSH) {
        map_rect(m_mx, m_my, m_mw, m_mh);
        const int ex = Fl::event_x(), ey = Fl::event_y();
        if (ex >= m_mx && ey >= m_my && ex < m_mx + m_mw && ey < m_my + m_mh) {
            /* Lowest index wins, as in the hardware priority order */
            for (int i = 0; i < FZ_SPRITE_COUNT; i++) {
                if (!m_data->on_screen(i)) {
                    continue;
                }
                int rx, ry, rw, rh;
                sprite_rect(i, rx, ry, rw, rh);
                if (ex >= rx && ey >= ry && ex < rx + rw && ey < ry + rh) {
                    m_data->selected = i;
                    redraw();
                    if (sheet) {
                        sheet->redraw();
                    }
                    return 1;
                }
            }
            m_data->selected = -1;
            redraw();
            if (sheet) {
                sheet->redraw();
            }
        }
        return 1;
    }
    return Fl_Widget::handle(event);
}

void SpriteInspector::map_rect(int& mx, int& my, int& mw, int& mh) const
{
    mx = x() + 12;
    my = y() + HEADER_H + 10;
    mw = w() - 24;
    mh = mw * 3 / 4;
}

void SpriteInspector::sprite_rect(int index, int& rx, int& ry, int& rw, int& rh) const
{
    const double sx = (double) m_mw / m_data->screen_w;
    const double sy = (double) m_mh / m_data->screen_h;
    rx = m_mx + (int) (m_data->screen_x(index) * sx);
    ry = m_my + (int) (m_data->screen_y(index) * sy);
    rw = (int) (FZ_SPRITE_W * sx) > 2 ? (int) (FZ_SPRITE_W * sx) : 2;
    const int h = m_data->height(index);
    rh = (int) (h * sy) > 2 ? (int) (h * sy) : 2;
}

} // namespace zui::video
