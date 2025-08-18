/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"

#define ZVB_COLOR_PALETTE_COUNT     (256)


/**
 * @brief Color type for the shader.
 */
typedef Vector3 zvb_color_t;


typedef struct {
    /* Raw array representing the colors in VRAM, each color is 2 bytes big */
    uint8_t raw_palette[ZVB_COLOR_PALETTE_COUNT * 2];
    /* Writes are now latched */
    int     wr_latch;
    Image   img_pal;
    Texture tex_pal;
    bool    dirty;
} zvb_palette_t;


/**
 * @brief Get the texture out of the palette
 */
static inline Texture zvb_pal_texture(zvb_palette_t* pal)
{
    return pal->tex_pal;
}


/**
 * @brief Initialize the palette, must be called before using it.
 */
void zvb_palette_init(zvb_palette_t* pal);


/**
 * @brief Function to call when a write occurs on the Palette memory area.
 *
 * @param addr Address relative to the palette address space.
 * @param data Byte to write in the palette
 */
void zvb_palette_write(zvb_palette_t* pal, uint32_t addr, uint8_t data);


/**
 * @brief Function to call when a read occurs on the Palette memory area.
 */
uint8_t zvb_palette_read(zvb_palette_t* pal, uint32_t addr);


/**
 * @brief Update the palette renderer, needs to be called before starting drawing anything on screen.
 */
void zvb_palette_update(zvb_palette_t* pal);
