/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/frame_convert.h"
#include <cstddef>

namespace zui::common {

void frame_to_rgb888(const uint16_t* src, int width, int height, int src_stride, uint8_t* dst)
{
    for (int y = 0; y < height; y++) {
        const uint16_t* row = src + (std::size_t) y * src_stride;
        for (int x = 0; x < width; x++) {
            const uint16_t p = row[x];
            const uint8_t r = (p >> 11) & 0x1F, g = (p >> 5) & 0x3F, b = p & 0x1F;
            dst[0] = (uint8_t) ((r << 3) | (r >> 2));
            dst[1] = (uint8_t) ((g << 2) | (g >> 4));
            dst[2] = (uint8_t) ((b << 3) | (b >> 2));
            dst += 3;
        }
    }
}

void rgb888_scale(const uint8_t* src, int src_width, int src_height,
                  uint8_t* dst, int dst_width, int dst_height, const int* xmap)
{
    for (int y = 0; y < dst_height; y++) {
        const uint8_t* row = src + (std::size_t) (y * src_height / dst_height) * src_width * 3;
        for (int x = 0; x < dst_width; x++) {
            const uint8_t* p = row + xmap[x] * 3;
            dst[0] = p[0];
            dst[1] = p[1];
            dst[2] = p[2];
            dst += 3;
        }
    }
}

} // namespace zui::common
