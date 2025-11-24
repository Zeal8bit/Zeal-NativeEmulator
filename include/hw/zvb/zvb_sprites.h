/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "raylib.h"


/**
 * @brief Number of sprites in the system
 */
#define ZVB_SPRITES_COUNT   (128)


/**
 * @brief Define the sprite organization as they are in the hardware
 */
typedef struct {
    uint16_t y;
    uint16_t x;
    /* Sprite's flag can be read as a byte or as a bitmap */
    union {
        struct {
            uint8_t tile_number : 8;
            uint8_t tileset_idx : 1;
            uint8_t behind_fg : 1;
            uint8_t flip_y : 1;
            uint8_t flip_x : 1;
            uint8_t palette : 4;
        } bitmap;
        uint16_t raw;
    } flags;
    union {
        struct {
            uint8_t reserved_0 : 1;
            uint8_t height_32  : 1;
            uint8_t reserved_2 : 6;
            uint8_t reserved_8 : 8;
        } bitmap;
        uint16_t raw;
    } extra_flags;
} zvb_sprite_t;

_Static_assert(sizeof(zvb_sprite_t) == 8, "Sprite structure must have a size of 8 bytes");


/**
 * @brief Sprite type where each entry is a float (for the texture)
 */
typedef struct {
    float x;
    float y;
    float tile_number;
    float palette;
    float f_behind_fg;
    float f_flip_y;
    float f_flip_x;
    float f_height_32;
} zvb_fsprite_t;


/**
 * @brief Define all the sprites in the system.
 */
typedef struct {
    zvb_sprite_t    data[ZVB_SPRITES_COUNT];
    zvb_fsprite_t   fdata[ZVB_SPRITES_COUNT];
    int             wr_latch;
#if CONFIG_USE_SHADERS
    /* Make rendering faster by using an Image and a Texture for both layers */
    Image           img_sprites;
    Texture         tex_sprites;
    int             dirty;
#endif
} zvb_sprites_t;


#if CONFIG_USE_SHADERS
/**
 * @brief Get the shader out of the sprites
 */
static inline Texture* zvb_sprites_texture(zvb_sprites_t* sprites)
{
    return &sprites->tex_sprites;
}
#endif


/**
 * @brief Initialize the sprites, must be called before using it.
 */
void zvb_sprites_init(zvb_sprites_t* sprites);


/**
 * @brief Function to call when a write occurs on the sprites memory area.
 *
 * @param addr Address relative to the sprites address space.
 * @param data Byte to write in the sprites
 */
void zvb_sprites_write(zvb_sprites_t* sprites, uint32_t addr, uint8_t data);


/**
 * @brief Function to call when a read occurs on the sprites memory area.
 *
 * @param layer Layer to read from (0 or 1)
 */
uint8_t zvb_sprites_read(zvb_sprites_t* sprites, uint32_t addr);


/**
 * @brief Update the sprites renderer, needs to be called before starting drawing anything on screen.
 */
void zvb_sprites_update(zvb_sprites_t* sprites);
