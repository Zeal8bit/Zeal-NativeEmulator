/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "video/sprite_sheet.h"
#include <cstdio>
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/machine.h"
#include "theme/theme.h"
#include "video/sprite_data.h"

namespace zui::video {

using namespace zui::theme;

SpriteSheet::SpriteSheet(int X, int Y, int W, int H, SpriteData* data)
    : Fl_Widget(X, Y, W, H), m_data(data)
{
    tooltip("Hover to inspect, click to pin. Dimmed sprites are off-screen.");
}

int SpriteSheet::card_w() const
{ return FZ_SPRITE_W * zoom + 14; }

int SpriteSheet::card_h() const
{ return FZ_SPRITE_MAX_H * zoom + 30; }

void SpriteSheet::relayout()
{
    const int W = COLUMNS * card_w() + 8;
    const int H = (FZ_SPRITE_COUNT / COLUMNS) * card_h() + 8 + (m_data->gfx ? 0 : 28);
    if (w() != W || h() != H) {
        size(W, H);
        if (parent()) parent()->redraw();
    }
    redraw();
}

void SpriteSheet::draw()
{
    fl_color(COLOR_BG);
    fl_rectf(x(), y(), w(), h());
    int top = y() + 4;
    if (!m_data->gfx) {
        fl_font(FL_HELVETICA, 12);
        fl_color(COLOR_AMBER);
        fl_draw("Text mode: sprites are only displayed in graphics modes.", x() + 8, top + 16);
        top += 28;
    }
    const int lh = data_line_height();
    for (int i = 0; i < FZ_SPRITE_COUNT; i++) {
        const int cx = x() + 4 + (i % COLUMNS) * card_w();
        const int cy = top + (i / COLUMNS) * card_h();
        if (cx + card_w() < x() || cy > y() + h()) continue;
        const fz_sprite_t& sp = m_data->info[i];
        const bool off = !sp.on_screen;
        fl_color(i == m_data->hover ? COLOR_HOVER : COLOR_SURFACE);
        fl_rectf(cx + 2, cy + 2, card_w() - 4, card_h() - 4);
        compose_sprite(*m_data, i, zoom, off, m_buf);
        fl_draw_image(m_buf.data(), cx + 7, cy + 7, FZ_SPRITE_W * zoom, FZ_SPRITE_MAX_H * zoom, 3);
        if (i == m_data->selected || i == m_data->hover) {
            fl_color(i == m_data->selected ? COLOR_AMBER : COLOR_CYAN);
            fl_rect(cx + 2, cy + 2, card_w() - 4, card_h() - 4);
        }
        char label[8];
        snprintf(label, sizeof(label), "%d", i);
        fl_font(DATA_FONT, DATA_SIZE);
        fl_color(off ? COLOR_DIM : COLOR_CYAN);
        fl_draw(label, cx + 7, cy + 7 + FZ_SPRITE_MAX_H * zoom + lh - 2);
    }
}

int SpriteSheet::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            m_data->hover = -1;
            redraw();
            if (inspector) inspector->redraw();
            return 1;
        case FL_MOVE: {
            const int i = sprite_at(Fl::event_x(), Fl::event_y());
            if (i != m_data->hover) {
                m_data->hover = i;
                redraw();
                if (inspector) inspector->redraw();
            }
            return 1;
        }
        case FL_PUSH: {
            const int i = sprite_at(Fl::event_x(), Fl::event_y());
            m_data->selected = (i == m_data->selected) ? -1 : i;
            redraw();
            if (inspector) inspector->redraw();
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

int SpriteSheet::sprite_at(int ex, int ey) const
{
    const int top = y() + 4 + (m_data->gfx ? 0 : 28);
    if (ex < x() + 4 || ey < top) return -1;
    const int col = (ex - x() - 4) / card_w();
    const int row = (ey - top) / card_h();
    if (col >= COLUMNS) return -1;
    const int i = row * COLUMNS + col;
    return i < FZ_SPRITE_COUNT ? i : -1;
}

} // namespace zui::video
