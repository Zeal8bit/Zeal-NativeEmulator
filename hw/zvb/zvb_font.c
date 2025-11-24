/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_font.h"

/* Make sure the default font has the correct size */
_Static_assert(sizeof(default_font) == DEFAULT_FONT_SIZE, "Default font has to have the same size as the font area in VRAM");

#if CONFIG_USE_SHADERS
/**
 * @brief Update the image with an incoming byte that must be interpreted as a bitmap
 */
static void font_update_img(zvb_font_t* font, uint32_t addr, uint_fast8_t data)
{
    /* Pixels of the image can be access as an array directly */
    Color *pixels = (Color*) font->img_font.data;

    /* The font can be seen as an array of characters where each character occupies ZVB_FONT_CHAR_SIZE bytes */
    const int char_idx = addr / ZVB_FONT_CHAR_SIZE;
    const int char_row = addr % ZVB_FONT_CHAR_SIZE;

    /* In the image, the characters are organized from left to right, pixel 8 is the first pixel of the
     * character index 1!
     * Thus, to access row R of the current character, we have to jump R*font->img_font.width pixels
     */
    const int pixels_idx = char_idx * ZVB_FONT_CHAR_WIDTH + char_row * font->img_font.width;

    for (int col = 0; col < ZVB_FONT_CHAR_WIDTH; col++) {
        /* Bit 7 is leftmost, bit 0 is rightmost */
        int is_white = (data >> (7 - col)) & 1;
        pixels[pixels_idx + col] = is_white ? WHITE : BLACK;
    }

    font->dirty = 1;
}
#endif // CONFIG_USE_SHADERS


void zvb_font_init(zvb_font_t* font)
{
    assert(font != NULL);

    /* Load the default font in memory */
    memcpy(font->raw_font, default_font, sizeof(font->raw_font));

#if CONFIG_USE_SHADERS
    /* Convert the bitmaps into a proper image  */
    font->img_font = GenImageColor(ZVB_FONT_CHAR_COUNT * ZVB_FONT_CHAR_WIDTH, ZVB_FONT_CHAR_HEIGHT, BLACK);

    /* Trigger an update to update the Image */
    for (size_t i = 0; i < sizeof(font->raw_font); i++) {
        font_update_img(font, i, font->raw_font[i]);
    }

    font->tex_font = LoadTextureFromImage(font->img_font);
    font->dirty = 0;
#endif // CONFIG_USE_SHADERS
}


void zvb_font_write(zvb_font_t* font, uint32_t addr, uint8_t data)
{
    font->raw_font[addr] = data;
#if CONFIG_USE_SHADERS
    font_update_img(font, addr, data);
#endif // CONFIG_USE_SHADERS
}


uint8_t zvb_font_read(zvb_font_t* font, uint32_t addr)
{
    return font->raw_font[addr];
}


void zvb_font_update(zvb_font_t* font)
{
#if CONFIG_USE_SHADERS
    if (font->dirty != 0) {
        UpdateTexture(font->tex_font, font->img_font.data);
        font->dirty = 0;
    }
#else
    /* Prevent a warning from the compiler */
    (void) font;
#endif
}


