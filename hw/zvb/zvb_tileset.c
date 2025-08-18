/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_tileset.h"


static void tileset_update_img(zvb_tileset_t* tileset, uint32_t addr, uint_fast8_t data);


void zvb_tileset_init(zvb_tileset_t* tileset)
{
    assert(tileset != NULL);

    /* Initialize both tilesets to 0 on boot (not reset) */
    memset(tileset->raw, 0, sizeof(tileset->raw));

    /* Create an empty bitmap image, to transmit the data faster to the GPU
     * Each tile is represented by 256 8-bit values on ZVB,
     * Since here the colors are 32-bit, we can store 4 pixels inside.
     * So each tile is 64-color big, we have 256 tiles. */
    const int width = 64;
    const int height = 256;
    _Static_assert(width*height*sizeof(Color) == 65536, "Image texture size is invalid");
    tileset->img_tileset = GenImageColor(width, height, BLACK);
    /* Set all the bytes to 0 */
    memset(tileset->img_tileset.data, 0, width * height);

    tileset->tex_tileset = LoadTextureFromImage(tileset->img_tileset);
    tileset->dirty = 0;
}


void zvb_tileset_write(zvb_tileset_t* tileset, uint32_t addr, uint8_t data)
{
    tileset->raw[addr] = data;
    tileset_update_img(tileset, addr, data);
}


uint8_t zvb_tileset_read(zvb_tileset_t* tileset, uint32_t addr)
{
    return tileset->raw[addr];
}


void zvb_tileset_update(zvb_tileset_t* tileset)
{
    if (tileset->dirty != 0) {
        UpdateTexture(tileset->tex_tileset, tileset->img_tileset.data);
        tileset->dirty = 0;
    }
}


/**
 * @brief Update the image with incoming byte
 */
static void tileset_update_img(zvb_tileset_t* tileset, uint32_t addr, uint_fast8_t data)
{
    _Static_assert(sizeof(Color) == 4, "Color should be 32-bit big");
    /* Pixels of the image can be access as an array directly. The lines of each tile
     * are organized linearly. */
    uint8_t *pixels = tileset->img_tileset.data;
    pixels[addr] = data;
    tileset->dirty = 1;
}
