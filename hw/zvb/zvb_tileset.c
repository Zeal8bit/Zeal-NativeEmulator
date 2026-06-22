/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_tileset.h"


static void tileset_update_img(zvb_tileset_t* tileset, uint32_t addr, uint_fast8_t data);


void zvb_tileset_init(zvb_tileset_t* tileset, bool rendering_enabled)
{
    assert(tileset != NULL);

    /* Initialize both tilesets to 0 on boot (not reset) */
    memset(tileset->raw, 0, sizeof(tileset->raw));

    if (!rendering_enabled) {
        tileset->dirty = 0;
        return;
    }

    /* One texture pixel maps to one tileset byte. Store data in the red channel
     * to avoid RGBA channel packing and platform-specific grayscale formats. */
    const int width = 256;
    const int height = 256;
    _Static_assert(width * height == sizeof(tileset->raw), "Image texture size is invalid");
    tileset->img_tileset = GenImageColor(width, height, BLACK);

    tileset->tex_tileset = LoadTextureFromImage(tileset->img_tileset);
    SetTextureFilter(tileset->tex_tileset, TEXTURE_FILTER_POINT);
    SetTextureWrap(tileset->tex_tileset, TEXTURE_WRAP_CLAMP);
    tileset->dirty = 0;
}


void zvb_tileset_write(zvb_tileset_t* tileset, uint32_t addr, uint8_t data)
{
    tileset->raw[addr] = data;
    if (tileset->img_tileset.data != NULL) {
        tileset_update_img(tileset, addr, data);
    }
}


uint8_t zvb_tileset_read(zvb_tileset_t* tileset, uint32_t addr)
{
    return tileset->raw[addr];
}


void zvb_tileset_update(zvb_tileset_t* tileset)
{
    if (tileset->dirty != 0 && tileset->tex_tileset.id != 0) {
        UpdateTexture(tileset->tex_tileset, tileset->img_tileset.data);
        tileset->dirty = 0;
    }
}


/**
 * @brief Update the image with incoming byte
 */
static void tileset_update_img(zvb_tileset_t* tileset, uint32_t addr, uint_fast8_t data)
{
    Color *pixels = tileset->img_tileset.data;
    pixels[addr].r = data;
    pixels[addr].g = 0;
    pixels[addr].b = 0;
    pixels[addr].a = 255;
    tileset->dirty = 1;
}
