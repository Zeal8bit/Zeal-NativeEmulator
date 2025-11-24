/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "raylib.h"
#include "utils/log.h"
#include "utils/helpers.h"
#include "utils/paths.h"
#include "hw/zvb/zvb.h"
#include "hw/zvb/zvb_render.h"
#include "hw/zvb/zvb_render_cpu.h"


void zvb_render_init(zvb_render_t* render, zvb_t* dev)
{
    render->img = GenImageColor(ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT, BLACK);
    render->tex = LoadTextureFromImage(render->img);
    log_printf("CPU rendering ready\n");
    (void) dev;
}


static void zvb_render_render_text_char(Color* fb, const uint8_t* char_data, int pos, Color bg, Color fg)
{
    /* Make the assumption that each character is 8-bit large */
    _Static_assert(ZVB_FONT_CHAR_WIDTH == 8, "Font characters must have a width of 8");
    for (int i = 0; i < ZVB_FONT_CHAR_HEIGHT; i++) {
        /* 8 pixels for the current line */
        const int pixels = *char_data++;
        for (int j = 0; j < ZVB_FONT_CHAR_WIDTH; j++) {
            /* The highest bit is the most-left pixel of the character */
            int pixel = (pixels >> (ZVB_FONT_CHAR_WIDTH - 1 - j)) & 1;
            if (pixel) {
                fb[pos + j] = fg;
            } else {
                fb[pos + j] = bg;
            }
        }
        /* Go to the next line without moving the `x` coordinate (column) */
        pos += ZVB_MAX_RES_WIDTH;
    }
}

void zvb_render_text_mode(zvb_render_t* render, zvb_t* zvb)
{
    int layer0;
    int bg_val;
    int fg_val;
    Color bg_col;
    Color fg_col;
    Color *framebuffer = (Color*) render->img.data;

    /* Get the visible amount of char on screen */
    int total_lines = 40;
    int total_cols  = 80;
    if (zvb->mode == MODE_TEXT_320) {
        total_lines /= 2;
        total_cols /= 2;
    }
    /* Get the cursor position and its color */
    zvb_text_info_t info;
    zvb_text_update(&zvb->text, &info);

    /* Browse each entry in the layer0 and layer1 */
    for (int i = 0; i < total_lines; i++) {
        for (int j = 0; j < total_cols; j++) {
            if (j == info.pos[0] && i == info.pos[1]) {
                /* Cursor! */
                layer0 = info.charidx;
                bg_val = info.color[0];
                fg_val = info.color[1];
            } else {
                /* Take into account the scrolling values ([0] = x, [1] = y) */
                const int col = (j + info.scroll[0]) % TEXT_MAXIMUM_COLUMNS;
                const int line = (i + info.scroll[1]) % TEXT_MAXIMUM_LINES;
                /* Calculate the index out of the line and column */
                int index = line * TEXT_MAXIMUM_COLUMNS + col;
                /* Get the entries from the layers (background and attributes) */
                layer0 = zvb->layers.raw_layer0[index];
                const int layer1 = zvb->layers.raw_layer1[index];
                /* layer1 represents two colors from the palette */
                bg_val = (layer1 >> 4) & 0xf;
                fg_val = (layer1 >> 0) & 0xf;
            }
            /* The position of the character on screen is unchanged, we need the PIXEL offset */
            const int x_pixel = j * ZVB_FONT_CHAR_WIDTH;
            const int y_pixel = i * ZVB_FONT_CHAR_HEIGHT;
            const int pos = y_pixel * ZVB_MAX_RES_WIDTH + x_pixel;
            /* layer0 is an index in the font array, where each byte is ZVB_FONT_CHAR_SIZE bytes big */
            const int char_idx = layer0 * ZVB_FONT_CHAR_SIZE;
            bg_col = zvb_palette_get_color(&zvb->palette, bg_val);
            fg_col = zvb_palette_get_color(&zvb->palette, fg_val);
            /* TODO: read 32-bit values instead to optimize it? */
            const uint8_t* char_data = &zvb->font.raw_font[char_idx];
            zvb_render_render_text_char(framebuffer, char_data, pos, bg_col, fg_col);
        }
    }

    /* Draw the resulting framebuffer */
    float scale = (zvb->mode == MODE_TEXT_320) ? 2.0f : 1.0f;
    UpdateTexture(render->tex, framebuffer);
    DrawTexturePro(
        render->tex,
        (Rectangle) { 0, 0, ZVB_MAX_RES_WIDTH / scale, -ZVB_MAX_RES_HEIGHT / scale },
        (Rectangle) { 0, 0, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT },  // dest rect
        (Vector2) { 0, 0 },
        0.0f,
        WHITE
    );
}


/**
 * @brief Render the debug textures when we are in text mode.
 * The `zvb_render` function must be called first.
 */
void zvb_render_debug_text_mode(zvb_render_t* render, zvb_t* zvb)
{
    (void) render;
    (void) zvb;
}


/**
 * @brief Render the screen in graphics mode
 */
void zvb_render_gfx_mode(zvb_render_t* render, zvb_t* zvb)
{
    (void) render;
    (void) zvb;
}


void zvb_render_debug_gfx_mode(zvb_render_t* render, zvb_t* zvb)
{
    (void) render;
    (void) zvb;
}


/**
 * @brief Render the screen in bitmap mode
 */
void zvb_render_bitmap_mode(zvb_render_t* render, zvb_t* zvb)
{
    Color border_color;
    int width = 0;
    int height = 0;
    int border_x = 0;
    int border_y = 0;
    Color* framebuffer = render->img.data;
    const uint8_t* bitmap = zvb->tileset.raw;

    /* In this mode, the screen resolution is half the maximum */
    const int screen_width = ZVB_MAX_RES_WIDTH / 2;
    const int screen_height = ZVB_MAX_RES_HEIGHT / 2;

    switch(zvb->mode) {
        case MODE_BITMAP_256:
            width = 256;
            height = 240;
            border_x = (screen_width - width) / 2;
            border_y = 0;
            break;
        case MODE_BITMAP_320:
            width = 320;
            height = 200;
            border_x = 0;
            border_y = (screen_height - height) / 2;
            break;
        default:
            return;
    }

    /* Last entry of the tileset is the border color */
    border_color = zvb_palette_get_color(&zvb->palette, bitmap[sizeof(zvb->tileset.raw) - 1]);

    /* Clear the framebuffer with the border color */
    for(int y = 0; y < screen_height; y++) {
        for(int x = 0; x < screen_width; x++) {
            /* The rendering area is smaller BUT the framebuffer's lines are still ZVB_MAX_RES_WIDTH pixels wide */
            framebuffer[y * ZVB_MAX_RES_WIDTH + x] = border_color;
        }
    }

    /* Fill the pixels in middle */
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            const int fb_x = x + border_x;
            const int fb_y = y + border_y;
            /* Index in the final framebuffer, including the borders */
            const int fb_index = fb_y * ZVB_MAX_RES_WIDTH + fb_x;
            /* Index of the pixel in the tileset */
            const int tileset_index = y * width + x;
            /* The entry is a reference to a color in the palette */
            const uint32_t col_idx = bitmap[tileset_index];
            framebuffer[fb_index] = zvb_palette_get_color(&zvb->palette, col_idx);
        }
    }
    UpdateTexture(render->tex, framebuffer);
    DrawTexturePro(
        render->tex,
        (Rectangle) { 0, 0, ZVB_MAX_RES_WIDTH / 2.0f, -ZVB_MAX_RES_HEIGHT / 2.0f },
        (Rectangle) { 0, 0, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT },
        (Vector2) { 0, 0 },
        0.0f,
        WHITE
    );
}
