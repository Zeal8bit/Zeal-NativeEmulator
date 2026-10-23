/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_font.h"


/* Make sure the default font has the correct size */
_Static_assert(sizeof(default_font) == DEFAULT_FONT_SIZE, "Default font has to have the same size as the font area in VRAM");

void zvb_font_init(zvb_font_t* font, bool rendering_enabled)
{
    assert(font != NULL);
    (void) rendering_enabled;

    /* Load the default font in memory */
    memcpy(font->raw_font, default_font, sizeof(font->raw_font));
}


void zvb_font_write(zvb_font_t* font, uint32_t addr, uint8_t data)
{
    font->raw_font[addr] = data;
}


uint8_t zvb_font_read(zvb_font_t* font, uint32_t addr)
{
    return font->raw_font[addr];
}

