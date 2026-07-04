/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "hw/zvb/zvb_sprites.h"

#define FLOATS_PER_SPRITE   8

#if ZVB_BLITTER_SHADER
/**
 * @brief Update the image with incoming byte from a given layer
 */
static void sprites_update_img(zvb_sprites_t* sprites, uint32_t idx)
{
    zvb_sprite_t*  sprite  = &sprites->data[idx];
    zvb_fsprite_t* fsprite = &sprites->fdata[idx];

    fsprite->y = sprite->y;
    fsprite->x = sprite->x;
    fsprite->tile_number = (sprite->flags.bitmap.tileset_idx << 8) | sprite->flags.bitmap.tile_number;
    fsprite->f_behind_fg = sprite->flags.bitmap.behind_fg;
    fsprite->f_flip_x = sprite->flags.bitmap.flip_x;
    fsprite->f_flip_y = sprite->flags.bitmap.flip_y;
    /* Store the palette as a mask to simplify the calculation in the shader */
    fsprite->palette = sprite->flags.bitmap.palette << 4;
    fsprite->f_height_32 = sprite->extra_flags.bitmap.height_32;
    sprites->dirty = 1;
}

void zvb_sprites_update(zvb_sprites_t* sprites)
{
    if (sprites->dirty != 0 && sprites->tex_sprites.id != 0) {
        UpdateTexture(sprites->tex_sprites, sprites->img_sprites.data);
        sprites->dirty = 0;
    }
}

#endif // ZVB_BLITTER_SHADER

void zvb_sprites_init(zvb_sprites_t* sprites, bool rendering_enabled)
{
    (void) rendering_enabled;
    assert(sprites != NULL);
    memset(sprites->data, 0, sizeof(sprites->data));
    sprites->wr_latch = 0;

        /* Init sorted-by-Y list: 0, 1, 2, ..., 127 */
    for (int i = 0; i < ZVB_SPRITES_COUNT; i++) {
        sprites->y_sorted[i] = i;
    }

#if ZVB_BLITTER_SHADER
    memset(sprites->fdata, 0, sizeof(sprites->fdata));

    if (!rendering_enabled) {
        sprites->dirty = 0;
        return;
    }

    /* Create an empty bitmap image that will be transfered to the GPU when rendering each frame */
    sprites->img_sprites = (Image) {
        .data = sprites->fdata,
        .width = ZVB_SPRITES_COUNT * 2,  // Two "pixels" per sprites
        .height = 1,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32
    };

    sprites->tex_sprites = LoadTextureFromImage(sprites->img_sprites);
    sprites->dirty = 0;
#endif
}

/**
 * @brief Re-sort y_sorted after sprite idx's Y changed.
 * Sort order: ascending Y, then ascending idx (lower idx = higher priority).
 */
static void zvb_sprites_resort_y(zvb_sprites_t* sprites, uint8_t idx)
{
    uint16_t new_y = sprites->data[idx].y;

    /* Find current position */
    int pos = -1;
    for (int i = 0; i < ZVB_SPRITES_COUNT; i++) {
        if (sprites->y_sorted[i] == idx) {
            pos = i;
            break;
        }
    }
    if (pos < 0) {
        return;
    }

    /* Remove */
    memmove(&sprites->y_sorted[pos], &sprites->y_sorted[pos + 1],
            ZVB_SPRITES_COUNT - 1 - pos);

    /* Insert at correct Y position (ascending Y, then idx for same Y) */
    int insert = ZVB_SPRITES_COUNT - 1;
    for (int i = 0; i < ZVB_SPRITES_COUNT - 1; i++) {
        uint16_t cur_y = sprites->data[sprites->y_sorted[i]].y;
        if (new_y < cur_y || (new_y == cur_y && idx < sprites->y_sorted[i])) {
            insert = i;
            break;
        }
    }
    memmove(&sprites->y_sorted[insert + 1], &sprites->y_sorted[insert],
            ZVB_SPRITES_COUNT - 1 - insert);
    sprites->y_sorted[insert] = idx;
}

int zvb_sprites_get_visible_sprites(zvb_sprites_t* sprites, int scanline, uint8_t sprites_idx[ZVB_SPRITES_COUNT])
{
    int count = 0;
    for (int si = 0; si < ZVB_SPRITES_COUNT; si++) {
        uint8_t idx = sprites->y_sorted[si];
        int sy = (int)sprites->data[idx].y - 16;
        if (sy > scanline)
            break;  /* Y-sorted, rest start below */
        int sh = sprites->data[idx].extra_flags.bitmap.height_32 ? 32 : 16;
        if (scanline < sy + sh)
            sprites_idx[count++] = idx;
    }
    return count;
}


void zvb_sprites_write(zvb_sprites_t* sprites, uint32_t addr, uint8_t data)
{
    /* Data is latched when writing the LSB */
    if ((addr & 1) == 0) {
        sprites->wr_latch = data;
    } else {
        uint8_t* raw_data = (uint8_t*) sprites->data;
        raw_data[addr - 1] = sprites->wr_latch;
        raw_data[addr] = data;

        /* Y field written (byte offset 1 within each 8-byte sprite) — re-sort */
        if ((addr & 0x7) == 1) {
            zvb_sprites_resort_y(sprites, addr / 8);
        }

#if ZVB_BLITTER_SHADER
        sprites_update_img(sprites, addr / sizeof(zvb_sprite_t));
#endif // ZVB_BLITTER_SHADER
    }
}


uint8_t zvb_sprites_read(zvb_sprites_t* sprites, uint32_t addr)
{
    /* No need to latch in this case */
    uint8_t* raw_data = (uint8_t*) sprites->data;
    return raw_data[addr];
}

