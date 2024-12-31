#pragma once

#include <stdint.h>
#include "raylib.h"
#include "hw/zvb/default_font.h"

/* Size of a character in the font, in bytes */
#define ZVB_FONT_CHAR_SIZE      (12)
/* Size of a character in the font in pixels */
#define ZVB_FONT_CHAR_WIDTH     (8)
#define ZVB_FONT_CHAR_HEIGHT    (12)
#define ZVB_FONT_CHAR_COUNT     (256)
#define ZVB_FONT_SIZE           (256 * ZVB_FONT_CHAR_SIZE)

/* Number of pixels in a single characters */
#define ZVB_FONT_CHAR_PIXELS    (ZVB_FONT_CHAR_WIDTH * ZVB_FONT_CHAR_HEIGHT)


typedef struct {
    /* Raw data, as organized in the real hardware. Each character is a bitmap. */
    uint8_t raw_font[ZVB_FONT_SIZE];
    /* Make rendering faster by using an Image and a Texture underneath */
    Image   img_font;
    Texture tex_font;
    int     dirty;
} zvb_font_t;


/**
 * @brief Get the texture out of the font
 */
static inline Texture zvb_font_texture(zvb_font_t* font)
{
    return font->tex_font;
}


/**
 * @brief Initialize the font, must be called before using it.
 */
void zvb_font_init(zvb_font_t* font);


/**
 * @brief Function to call when a write occurs on the Font memory area.
 * 
 * @param addr Address relative to the font address space.
 * @param data Byte to write in the font
 */
void zvb_font_write(zvb_font_t* font, uint32_t addr, uint8_t data);


/**
 * @brief Function to call when a read occurs on the Font memory area.
 */
uint8_t zvb_font_read(zvb_font_t* font, uint32_t addr);


/**
 * @brief Update the font renderer, needs to be called before starting drawing anything on screen.
 */
void zvb_font_update(zvb_font_t* font);
