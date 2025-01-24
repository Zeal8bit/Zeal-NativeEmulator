#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "hw/zvb/zvb_sprites.h"

#define FLOATS_PER_SPRITE   8

static void sprites_update_img(zvb_sprites_t* sprites, uint32_t idx);


void zvb_sprites_init(zvb_sprites_t* sprites)
{
    assert(sprites != NULL);
    memset(sprites->data, 0, sizeof(sprites->data));

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
}


void zvb_sprites_write(zvb_sprites_t* sprites, uint32_t addr, uint8_t data)
{
    uint8_t* raw_data = (uint8_t*) sprites->data;
    raw_data[addr] = data;
    sprites_update_img(sprites, addr / sizeof(zvb_sprite_t));
}


uint8_t zvb_sprites_read(zvb_sprites_t* sprites, uint32_t addr)
{
    (void) sprites;
    (void) addr;
    return 0;
}


void zvb_sprites_update(zvb_sprites_t* sprites)
{
    if (sprites->dirty != 0) {
        UpdateTexture(sprites->tex_sprites, sprites->img_sprites.data);
        sprites->dirty = 0;
    }
}


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
    fsprite->palette = sprite->flags.bitmap.palette;
    sprites->dirty = 1;
}
