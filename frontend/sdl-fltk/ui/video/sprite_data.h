/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* One hardware sprite as the sprite viewers show it. The raw attribute table
 * is read directly from the ZVB; positions, height and tileset index are
 * derived on demand instead of being copied into a parallel structure. */
#pragma once

#include <cstdint>
#include <vector>
#include "frontend/sdl-fltk/frontend.h"

namespace zui::video {

struct SpriteData {
    zvb_sprite_t info[FZ_SPRITE_COUNT] = {};
    uint8_t rgb[FZ_SPRITE_COUNT][FZ_SPRITE_W * FZ_SPRITE_MAX_H * 3] = {};
    uint8_t mask[FZ_SPRITE_COUNT][FZ_SPRITE_W * FZ_SPRITE_MAX_H] = {};
    bool gfx = false;
    bool color_4bit = false;
    int screen_w = 640;
    int screen_h = 480;
    int hover = -1;
    int selected = -1;

    int current() const { return selected >= 0 ? selected : hover; }
    int height(int i) const { return zvb_sprite_height(&info[i]); }
    int screen_x(int i) const { return (int) info[i].x - 16; }
    int screen_y(int i) const { return (int) info[i].y - 16; }
    uint16_t tile(int i) const { return (uint16_t) zvb_sprite_tile(&info[i], color_4bit); }
    uint8_t palette(int i) const { return info[i].flags.bitmap.palette; }
    bool flip_x(int i) const { return info[i].flags.bitmap.flip_x != 0; }
    bool flip_y(int i) const { return info[i].flags.bitmap.flip_y != 0; }
    bool behind_fg(int i) const { return info[i].flags.bitmap.behind_fg != 0; }
    uint16_t raw_flags(int i) const { return info[i].flags.raw; }
    uint16_t raw_extra(int i) const { return info[i].extra_flags.raw; }
    bool on_screen(int i) const
    {
        if (!gfx) {
            return false;
        }
        const int h = height(i), sx = screen_x(i), sy = screen_y(i);
        return sx < screen_w && sx + 16 > 0 && sy < screen_h && sy + h > 0;
    }

    void update();
};

/** Sprite pixels as RGB at `zoom`, dimmed when `dim` (off-screen sprites). */
void compose_sprite(const SpriteData& d, int index, int zoom, bool dim, std::vector<uint8_t>& out);

} // namespace zui::video
