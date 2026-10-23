/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_tilemap.h"


void zvb_tilemap_init(zvb_tilemap_t* tilemap, bool rendering_enabled)
{
    (void) rendering_enabled;
    assert(tilemap != NULL);

    /* Initialize both tilemaps to 0 on boot (not reset) */
    memset(tilemap->raw_layer0, 0, sizeof(tilemap->raw_layer0));
    memset(tilemap->raw_layer1, 0, sizeof(tilemap->raw_layer1));
}


void zvb_tilemap_write(zvb_tilemap_t* tilemap, int layer, uint32_t addr, uint8_t data)
{
    if (layer == 0) {
        tilemap->raw_layer0[addr] = data;
    } else {
        tilemap->raw_layer1[addr] = data;
    }
}


uint8_t zvb_tilemap_read(zvb_tilemap_t* tilemap, int layer, uint32_t addr)
{
    if (layer == 0) {
        return tilemap->raw_layer0[addr];
    } else {
        return tilemap->raw_layer1[addr];
    }
}
