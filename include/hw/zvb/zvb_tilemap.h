/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "raylib.h"

/**
 * @brief Size of each tilemap, in bytes
 */
#define ZVB_TILEMAP_SIZE        (3200)


typedef struct {
    /* Raw arrays representing the tilemaps in VRAM */
    uint8_t raw_layer0[ZVB_TILEMAP_SIZE];
    uint8_t raw_layer1[ZVB_TILEMAP_SIZE];
#if CONFIG_USE_SHADERS
    /* Make rendering faster by using an Image and a Texture for both layers */
    Image   img_tilemap;
    Texture tex_tilemap;
    int     dirty;
#endif
} zvb_tilemap_t;



#if CONFIG_USE_SHADERS
/**
 * @brief Get the shader out of the palette
 */
static inline Texture* zvb_tilemap_texture(zvb_tilemap_t* tilemap)
{
    return &tilemap->tex_tilemap;
}
#endif


/**
 * @brief Initialize the tilemap, must be called before using it.
 */
void zvb_tilemap_init(zvb_tilemap_t* tilemap);


/**
 * @brief Function to call when a write occurs on the tilemap memory area.
 *
 * @param layer Layer to write to (0 or 1)
 * @param addr Address relative to the tilemap address space.
 * @param data Byte to write in the tilemap
 */
void zvb_tilemap_write(zvb_tilemap_t* tilemap, int layer, uint32_t addr, uint8_t data);


/**
 * @brief Function to call when a read occurs on the tilemap memory area.
 *
 * @param layer Layer to read from (0 or 1)
 */
uint8_t zvb_tilemap_read(zvb_tilemap_t* tilemap, int layer, uint32_t addr);


/**
 * @brief Update the tilemap renderer, needs to be called before starting drawing anything on screen.
 */
void zvb_tilemap_update(zvb_tilemap_t* tilemap);
