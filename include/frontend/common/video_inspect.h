/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "hw/zvb/zvb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Packed RGB888, valid until the next render of this view or cleanup. */
typedef struct {
    const uint8_t* pixels;
    int width;
    int height;
    /* Grid cell size excluding the one-pixel grid line. */
    int cell_width;
    int cell_height;
    int columns;
    int items;
} video_inspect_image_t;

#define VIDEO_INSPECT_SPRITE_WIDTH ZVB_SPRITE_TILE_W
#define VIDEO_INSPECT_SPRITE_HEIGHT (2 * ZVB_SPRITE_TILE_H)

bool video_inspect_render(zvb_t* zvb, dbg_vram_t view, video_inspect_image_t* out);
bool video_inspect_sprite(zvb_t* zvb, int index, uint8_t* rgb, uint8_t* mask);
void video_inspect_free(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
