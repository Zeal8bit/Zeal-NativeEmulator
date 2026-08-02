/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "hw/zvb/zvb.h"

#if CONFIG_ENABLE_DEBUGGER

#define CHAR_W 8
#define CHAR_H 12
#define TILE_W 16
#define TILE_H 16
#define GRID_COLOR ((Color){ 166, 0, 0, 255 })

static Color palette_color(const zvb_t* zvb, int index)
{
    const uint8_t* raw = &zvb->palette.raw_palette[index * 2];
    const uint16_t rgb = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
    return (Color) {
        (uint8_t)((((rgb >> 11) & 31) * 255) / 31),
        (uint8_t)((((rgb >> 5) & 63) * 255) / 63),
        (uint8_t)(((rgb & 31) * 255) / 31),
        255
    };
}

static bool font_pixel(const zvb_t* zvb, int character, int x, int y)
{
    return ((zvb->font.raw_font[character * CHAR_H + y] >> (7 - x)) & 1) != 0;
}

static Color debug_pixel(const zvb_t* zvb, dbg_vram_t view, int x, int y)
{
    int cell_w = TILE_W + 1;
    int cell_h = TILE_H + 1;
    int columns = 16;
    int rows = 16;

    if (view == DBG_TILEMAP_LAYER0 || view == DBG_TILEMAP_LAYER1) {
        cell_w = CHAR_W + 1;
        cell_h = CHAR_H + 1;
        columns = 80;
        rows = 40;
    } else if (view == DBG_FONT) {
        cell_w = CHAR_W + 1;
        cell_h = CHAR_H + 1;
    }

    const int cell_x = x / cell_w;
    const int cell_y = y / cell_h;
    const int in_x = x % cell_w;
    const int in_y = y % cell_h;
    if (cell_x >= columns || cell_y >= rows) return BLANK;
    if (in_x == 0 || in_y == 0) return GRID_COLOR;

    if (view == DBG_PALETTE) {
        return palette_color(zvb, cell_y * 16 + cell_x);
    }

    if (view == DBG_FONT) {
        const int character = cell_y * 16 + cell_x;
        return font_pixel(zvb, character, in_x - 1, in_y - 1) ? WHITE : BLACK;
    }

    if (view == DBG_TILESET) {
        const int tile = cell_y * 16 + cell_x;
        const int offset = (in_y - 1) * TILE_W + in_x - 1;
        return palette_color(zvb, zvb->tileset.raw[tile * 256 + offset]);
    }

    const int index = cell_y * 80 + cell_x;
    const uint8_t attr = zvb->layers.raw_layer1[index];
    const Color fg = palette_color(zvb, attr & 0x0f);
    const Color bg = palette_color(zvb, (attr >> 4) & 0x0f);
    if (view == DBG_TILEMAP_LAYER1) {
        return in_x - 1 < CHAR_W / 2 ? bg : fg;
    }
    const uint8_t character = zvb->layers.raw_layer0[index];
    return font_pixel(zvb, character, in_x - 1, in_y - 1) ? fg : bg;
}

void zvb_render_debug_textures_cpu(zvb_t* zvb)
{
    for (int view = DBG_TILEMAP_LAYER0; view <= DBG_FONT; view++) {
        RenderTexture2D* target = &zvb->debug_tex[view];
        const int width = target->texture.width;
        const int height = target->texture.height;
        Color* pixels = malloc((size_t)width * height * sizeof(*pixels));
        if (pixels == NULL) continue;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                pixels[y * width + x] = debug_pixel(zvb, (dbg_vram_t)view, x, y);
            }
        }
        UpdateTexture(target->texture, pixels);
        free(pixels);
    }
}

#endif
