/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "raylib.h"

/**
 * @brief Size of the tileset, in bytes
 */
#define ZVB_TILESET_SIZE        (65536)


typedef struct {
    /* Raw arrays representing the tileset in VRAM */
    uint8_t raw[ZVB_TILESET_SIZE];
#if CONFIG_USE_SHADERS
    /* Make rendering faster by using an Image and a Texture for the tileset */
    Image   img_tileset;
    Texture tex_tileset;
    int     dirty;
#endif
} zvb_tileset_t;


#if CONFIG_USE_SHADERS
/**
 * @brief Get the texture out of the palette
 */
static inline Texture* zvb_tileset_texture(zvb_tileset_t* tileset)
{
    return &tileset->tex_tileset;
}
#endif

/**
 * @brief Initialize the tileset, must be called before using it.
 */
void zvb_tileset_init(zvb_tileset_t* tileset);


/**
 * @brief Function to call when a write occurs on the tileset memory area.
 *
 * @param layer Layer to write to (0 or 1)
 * @param addr Address relative to the tileset address space.
 * @param data Byte to write in the tileset
 */
void zvb_tileset_write(zvb_tileset_t* tileset, uint32_t addr, uint8_t data);


/**
 * @brief Function to call when a read occurs on the tileset memory area.
 *
 * @param layer Layer to read from (0 or 1)
 */
uint8_t zvb_tileset_read(zvb_tileset_t* tileset, uint32_t addr);


/**
 * @brief Update the tileset renderer, needs to be called before starting drawing anything on screen.
 */
void zvb_tileset_update(zvb_tileset_t* tileset);
