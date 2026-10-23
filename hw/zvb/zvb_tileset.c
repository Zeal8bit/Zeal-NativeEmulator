/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_tileset.h"


void zvb_tileset_init(zvb_tileset_t* tileset, bool rendering_enabled)
{
    assert(tileset != NULL);

    /* Initialize both tilesets to 0 on boot (not reset) */
    memset(tileset->raw, 0, sizeof(tileset->raw));

    (void) rendering_enabled;
}


void zvb_tileset_write(zvb_tileset_t* tileset, uint32_t addr, uint8_t data)
{
    tileset->raw[addr] = data;
}


uint8_t zvb_tileset_read(zvb_tileset_t* tileset, uint32_t addr)
{
    return tileset->raw[addr];
}
