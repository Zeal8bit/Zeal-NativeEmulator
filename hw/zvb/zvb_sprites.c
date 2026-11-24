/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_sprites.h"
#include "hw/zvb/zvb_tileset.h"

/* Compile-time layout checks */
_Static_assert(sizeof(zvb_sprite_t) == 8, "Sprite structure must have a size of 8 bytes");


uint8_t zvb_sprite_pixel(const zvb_sprite_t* sp, bool color_4bit,
                         const uint8_t* tileset, int x, int y)
{
    const int height = zvb_sprite_height(sp);
    if (x < 0 || x >= ZVB_SPRITE_TILE_W || y < 0 || y >= height) {
        return 0;
    }
    if (sp->flags.bitmap.flip_x) {
        x = ZVB_SPRITE_TILE_W - 1 - x;
    }
    if (sp->flags.bitmap.flip_y) {
        y = height - 1 - y;
    }
    const uint32_t tile = zvb_sprite_tile(sp, color_4bit);
    const int offset = y * ZVB_SPRITE_TILE_W + x;
    if (color_4bit) {
        const int byte_idx = (int) tile * ZVB_SPRITE_TILE_BYTES + offset;
        const uint8_t byte = tileset[(byte_idx / 2) & (ZVB_TILESET_SIZE - 1)];
        return (byte_idx & 1) ? (byte & 0x0f) : (byte >> 4);
    }
    return tileset[(tile * ZVB_SPRITE_TILE_BYTES + offset) & (ZVB_TILESET_SIZE - 1)];
}

void zvb_sprites_init(zvb_sprites_t* sprites, bool rendering_enabled)
{
    (void) rendering_enabled;
    assert(sprites != NULL);
    memset(sprites->data, 0, sizeof(sprites->data));
    sprites->wr_latch = 0;
}

int zvb_sprites_get_visible_sprites(zvb_sprites_t* sprites, int scanline, uint8_t sprites_idx[ZVB_SPRITES_COUNT])
{
    int count = 0;
    for (int idx = 0; idx < ZVB_SPRITES_COUNT && count < ZVB_SPRITES_PER_LINE; idx++) {
        int sy = (int)sprites->data[idx].y - 16;
        int sh = sprites->data[idx].extra_flags.bitmap.height_32 ? 32 : 16;
        if (scanline >= sy && scanline < sy + sh) {
            sprites_idx[count++] = idx;
        }
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

    }
}


uint8_t zvb_sprites_read(zvb_sprites_t* sprites, uint32_t addr)
{
    /* No need to latch in this case */
    uint8_t* raw_data = (uint8_t*) sprites->data;
    return raw_data[addr];
}
