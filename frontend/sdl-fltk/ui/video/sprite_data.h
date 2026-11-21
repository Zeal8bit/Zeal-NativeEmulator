/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* One hardware sprite as the sprite viewers show it. */
#pragma once

#include <cstdint>
#include <vector>
#include "common/machine.h"
#include <cstring>

namespace zui::video {

struct SpriteData {
    fz_sprite_t info[FZ_SPRITE_COUNT] = {};
    uint8_t rgb[FZ_SPRITE_COUNT][FZ_SPRITE_W * FZ_SPRITE_MAX_H * 3] = {};
    uint8_t mask[FZ_SPRITE_COUNT][FZ_SPRITE_W * FZ_SPRITE_MAX_H] = {};
    bool gfx = false;
    int screen_w = 640;
    int screen_h = 480;
    int hover = -1;
    int selected = -1;

    int current() const { return selected >= 0 ? selected : hover; }

    void update()
    {
        gfx = fz_gfx_screen(&screen_w, &screen_h);
        for (int i = 0; i < FZ_SPRITE_COUNT; i++) {
            if (!fz_sprite(i, &info[i], rgb[i], mask[i])) memset(&info[i], 0, sizeof(info[i]));
        }
    }
};

/** Sprite pixels as RGB at `zoom`, dimmed when `dim` (off-screen sprites). */
void compose_sprite(const SpriteData& d, int index, int zoom, bool dim, std::vector<uint8_t>& out);

} // namespace zui::video
