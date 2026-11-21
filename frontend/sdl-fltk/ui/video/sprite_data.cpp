/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "video/sprite_data.h"
#include <cstdint>
#include <vector>
#include "common/machine.h"

namespace zui::video {

/** Compose a sprite over a checkerboard, scaled, into an RGB buffer. */
void compose_sprite(const SpriteData& d, int index, int zoom, bool dim, std::vector<uint8_t>& out)
{
    const int W = FZ_SPRITE_W * zoom;
    const int H = FZ_SPRITE_MAX_H * zoom;
    out.resize((size_t) W * H * 3);
    const int check = zoom * 4;
    const int height = d.info[index].height;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            const int sx = x / zoom, sy = y / zoom;
            uint8_t r, g, b;
            if (sy >= height) {
                r = g = b = 0x18; /* outside a 16px-high sprite */
            } else if (d.mask[index][sy * FZ_SPRITE_W + sx]) {
                const uint8_t* p = &d.rgb[index][(sy * FZ_SPRITE_W + sx) * 3];
                r = p[0]; g = p[1]; b = p[2];
            } else {
                const bool light = ((x / check) + (y / check)) & 1;
                r = g = b = light ? 0x30 : 0x26;
            }
            if (dim) { r /= 3; g /= 3; b /= 3; }
            uint8_t* o = &out[((size_t) y * W + x) * 3];
            o[0] = r; o[1] = g; o[2] = b;
        }
    }
}

} // namespace zui::video
