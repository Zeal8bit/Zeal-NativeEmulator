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
    uint16_t padding;
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
    float padding;
} zvb_fsprite_t;


/**
 * @brief Define all the sprites in the system.
 */
typedef struct {
    zvb_sprite_t    data[ZVB_SPRITES_COUNT];
    zvb_fsprite_t   fdata[ZVB_SPRITES_COUNT];
    /* Make rendering faster by using an Image and a Texture for both layers */
    Image           img_sprites;
    Texture         tex_sprites;
    int             dirty;
} zvb_sprites_t;


/**
 * @brief Get the shader out of the sprites
 */
static inline Texture* zvb_sprites_texture(zvb_sprites_t* sprites)
{
    return &sprites->tex_sprites;
}


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
