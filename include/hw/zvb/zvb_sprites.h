/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Number of sprites in the system
 */
#define ZVB_SPRITES_COUNT   (128)
/* Emulator approximation of a variable rendering budget. Hardware capacity
 * depends on sprite order and coverage, rather than a fixed sprite count. */
#define ZVB_SPRITES_PER_LINE (40)


/**
 * @brief Define the sprite organization as they are in the hardware
 */

#ifdef __cplusplus
extern "C" {
#endif

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



/**
 * @brief Define all the sprites in the system.
 */
typedef struct {
    zvb_sprite_t    data[ZVB_SPRITES_COUNT];
    int             wr_latch;
} zvb_sprites_t;

#define ZVB_SPRITE_TILE_W       16
#define ZVB_SPRITE_TILE_H       16
#define ZVB_SPRITE_TILE_BYTES   (ZVB_SPRITE_TILE_W * ZVB_SPRITE_TILE_H)

/* Height in pixels: sprites are 16x16 or 16x32. */
static inline int zvb_sprite_height(const zvb_sprite_t* sp)
{
    return sp->extra_flags.bitmap.height_32 ? 32 : 16;
}

/* Tileset index, including the high bank used by 4-bit modes. */
static inline uint32_t zvb_sprite_tile(const zvb_sprite_t* sp, bool color_4bit)
{
    const uint32_t tile = sp->flags.bitmap.tile_number;
    return tile + ((sp->flags.bitmap.tileset_idx && color_4bit) ? 256 : 0);
}


/**
 * @brief Initialize the sprites, must be called before using it.
 */
void zvb_sprites_init(zvb_sprites_t* sprites, bool rendering_enabled);

/**
 * @brief Colour index of a sprite pixel at local coordinates, after flipping.
 * @return 0 for a transparent pixel, otherwise the tileset colour index
 */
uint8_t zvb_sprite_pixel(const zvb_sprite_t* sp, bool color_4bit,
                         const uint8_t* tileset, int x, int y);


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
 * @brief Get the list of visible sprites for a given scanline, sorted by priority.
 * 
 * @returns Number of sprites visible on the scanline, and fills the `out_sprites` array with the visible sprites.
 */
int zvb_sprites_get_visible_sprites(zvb_sprites_t* sprites, int scanline, uint8_t sprites_idx[ZVB_SPRITES_COUNT]);

#ifdef __cplusplus
} /* extern "C" */
#endif
