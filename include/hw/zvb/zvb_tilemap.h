/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Size of each tilemap, in bytes
 */
#define ZVB_TILEMAP_SIZE        (3200)


typedef struct {
    /* Raw arrays representing the tilemaps in VRAM */
    uint8_t raw_layer0[ZVB_TILEMAP_SIZE];
    uint8_t raw_layer1[ZVB_TILEMAP_SIZE];
} zvb_tilemap_t;


/**
 * @brief Initialize the tilemap, must be called before using it.
 */
void zvb_tilemap_init(zvb_tilemap_t* tilemap, bool rendering_enabled);


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
