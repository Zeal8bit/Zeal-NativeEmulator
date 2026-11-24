/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* RGB565 frame conversion and nearest-neighbour scaling for the screen. */
#pragma once

#include <cstdint>

namespace zui::common {

/* Expand an RGB565 frame to packed RGB888. Source rows are `src_stride`
 * pixels apart: the emulator framebuffer is always FZ_SCREEN_WIDTH wide,
 * even for 320-pixel modes. */
void frame_to_rgb888(const uint16_t* src, int width, int height, int src_stride, uint8_t* dst);

/* Nearest-neighbour scale a packed RGB888 frame. xmap maps each destination
 * column to a source column (0 <= xmap[x] < src_width). */
void rgb888_scale(const uint8_t* src, int src_width, int src_height,
                  uint8_t* dst, int dst_width, int dst_height, const int* xmap);

} // namespace zui::common
