/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "hw/zvb/zvb.h"

#if CONFIG_ENABLE_DEBUGGER

#define CHAR_W 8
#define CHAR_H 12
#define TILE_W 16
#define TILE_H 16
#define GRID_COLOR      ((Color){ 166, 0, 0, 255 })
#define SUBSCREEN_COLOR ((Color){ 0, 165, 0, 255 })
#define GREY_LIGHT      ((Color){ 204, 204, 204, 255 })
#define GREY_DARK       ((Color){ 127, 127, 127, 255 })

static Color palette_color(const zvb_t* zvb, int index)
{
    const uint8_t* raw = &zvb->palette.raw_palette[index * 2];
    const uint16_t rgb = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
    return (Color) {
        (uint8_t)((((rgb >> 11) & 31) * 255) / 31),
        (uint8_t)((((rgb >> 5) & 63) * 255) / 63),
        (uint8_t)(((rgb & 31) * 255) / 31),
        255
    };
}

/* Grid geometry of a debug view */
typedef struct {
    int  cell_w;      /* cell size including the 1px grid border */
    int  cell_h;
    int  columns;     /* number of cells */
    int  rows;
    bool layer_view;  /* layer0/layer1 views (text or gfx) */
} dbg_grid_t;

static dbg_grid_t debug_grid(dbg_vram_t view, bool gfx_mode)
{
    dbg_grid_t g = {
        .cell_w = TILE_W + 1,
        .cell_h = TILE_H + 1,
        .columns = 16,
        .rows = 16,
    };

    if (view == DBG_TILEMAP_LAYER0 || view == DBG_TILEMAP_LAYER1) {
        /* Graphics mode: layers are 16x16 tiles, text mode: 8x12 characters */
        g.cell_w = gfx_mode ? TILE_W + 1 : CHAR_W + 1;
        g.cell_h = gfx_mode ? TILE_H + 1 : CHAR_H + 1;
        g.columns = 80;
        g.rows = 40;
        g.layer_view = true;
    } else if (view == DBG_FONT) {
        g.cell_w = CHAR_W + 1;
        g.cell_h = CHAR_H + 1;
    } else if (view == DBG_TILESET) {
        /* The tileset texture holds 32 rows (512 tiles in 4-bit mode) */
        g.rows = 32;
    }
    return g;
}

/* ------------------------------------------------------------------ */
/*  Fast image helpers                                                 */
/* ------------------------------------------------------------------ */

/* Write a single pixel in the image buffer directly. */
static inline void img_put_pixel(Image* img, int x, int y, Color color)
{
    ((Color*)img->data)[y * img->width + x] = color;
}

/* Fill the whole image buffer with one color. */
static inline void img_fill(Image* img, Color color)
{
    Color* data = img->data;
    const int count = img->width * img->height;
    for (int i = 0; i < count; i++) {
        data[i] = color;
    }
}

/* Fill a rectangle in the image buffer directly. */
static inline void img_fill_rect(Image* img, int x, int y, int w, int h, Color color)
{
    Color* data = img->data;
    for (int j = 0; j < h; j++) {
        Color* row = &data[(y + j) * img->width + x];
        for (int i = 0; i < w; i++) {
            row[i] = color;
        }
    }
}

/* Draw the 1px red grid. Views whose grid fills the whole texture (tileset/
 * palette/font, and the layers in graphics mode) also get the right/bottom
 * border line. */
static void debug_draw_grid(Image* img, const dbg_grid_t* g, bool gfx_mode)
{
    const bool fills = !g->layer_view || gfx_mode;
    const int x_lines = fills ? g->columns : g->columns - 1;
    const int y_lines = fills ? g->rows : g->rows - 1;
    const int width = x_lines * g->cell_w + 1;
    const int height = y_lines * g->cell_h + 1;
    Color* data = img->data;

    for (int i = 0; i <= x_lines; i++) {
        const int x = i * g->cell_w;
        for (int y = 0; y < height; y++) {
            data[y * img->width + x] = GRID_COLOR;
        }
    }
    for (int j = 0; j <= y_lines; j++) {
        const int y = j * g->cell_h;
        for (int x = 0; x < width; x++) {
            data[y * img->width + x] = GRID_COLOR;
        }
    }
}

/* Graphics-mode layers: highlight the subscreen (320x240) boundaries, one
 * green line at every 20th/15th cell, like the former debug shader. */
static void debug_draw_subscreens(Image* img, const dbg_grid_t* g)
{
    Color* data = img->data;
    const int width = g->columns * g->cell_w + 1;
    const int height = g->rows * g->cell_h + 1;

    for (int i = 0; i * 20 <= g->columns; i++) {
        const int x = i * 20 * g->cell_w;
        for (int y = 0; y < height; y++) {
            data[y * img->width + x] = SUBSCREEN_COLOR;
        }
    }
    for (int j = 0; j * 15 <= g->rows; j++) {
        const int y = j * 15 * g->cell_h;
        for (int x = 0; x < width; x++) {
            data[y * img->width + x] = SUBSCREEN_COLOR;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Palette: 256 solid-color rectangles                                */
/* ------------------------------------------------------------------ */

static void render_palette(zvb_t* zvb)
{
    Image* img = &zvb->debug_img[DBG_PALETTE];
    const dbg_grid_t g = debug_grid(DBG_PALETTE, false);

    img_fill(img, BLANK);
    debug_draw_grid(img, &g, false);

    for (int y = 0; y < g.rows; y++) {
        for (int x = 0; x < g.columns; x++) {
            img_fill_rect(img, x * g.cell_w + 1, y * g.cell_h + 1,
                          TILE_W, TILE_H, palette_color(zvb, y * 16 + x));
        }
    }
    UpdateTexture(zvb->debug_tex[DBG_PALETTE], img->data);
}

/* ------------------------------------------------------------------ */
/*  Font: 256 characters (8x12 bitmaps)                                */
/* ------------------------------------------------------------------ */

static void render_font(zvb_t* zvb)
{
    Image* img = &zvb->debug_img[DBG_FONT];
    const dbg_grid_t g = debug_grid(DBG_FONT, false);

    img_fill(img, BLACK);
    debug_draw_grid(img, &g, false);

    for (int y = 0; y < g.rows; y++) {
        for (int x = 0; x < g.columns; x++) {
            const int character = y * 16 + x;
            const int px = x * g.cell_w + 1;
            const int py = y * g.cell_h + 1;
            /* Browse the 12 bytes of the character, then decompose the bits */
            const uint8_t* raw = &zvb->font.raw_font[character * CHAR_H];
            for (int cy = 0; cy < CHAR_H; cy++) {
                uint8_t bits = raw[cy];
                for (int cx = 0; cx < CHAR_W; cx++) {
                    if (bits & 0x80) {
                        img_put_pixel(img, px + cx, py + cy, WHITE);
                    }
                    bits <<= 1;
                }
            }
        }
    }
    UpdateTexture(zvb->debug_tex[DBG_FONT], img->data);
}

/* ------------------------------------------------------------------ */
/*  Tileset: 16x32 tiles (16x16 each)                                  */
/* ------------------------------------------------------------------ */

static void render_tileset(zvb_t* zvb)
{
    Image* img = &zvb->debug_img[DBG_TILESET];
    const dbg_grid_t g = debug_grid(DBG_TILESET, false);
    const bool color_4bit = zvb->mode == MODE_GFX_640_4BIT || zvb->mode == MODE_GFX_320_4BIT;

    img_fill(img, BLANK);
    debug_draw_grid(img, &g, false);

    for (int ty = 0; ty < g.rows; ty++) {
        for (int tx = 0; tx < g.columns; tx++) {
            const int tile = ty * 16 + tx;
            const int px = tx * g.cell_w + 1;
            const int py = ty * g.cell_h + 1;

            if (color_4bit) {
                /* 4-bit tiles are packed two pixels per byte */
                for (int yy = 0; yy < TILE_H; yy++) {
                    for (int xx = 0; xx < TILE_W; xx++) {
                        const int final_idx = tile * 256 + yy * TILE_W + xx;
                        const uint8_t byte = zvb->tileset.raw[final_idx / 2];
                        const int color = (final_idx & 1) ? (byte & 0x0f) : (byte >> 4);
                        img_put_pixel(img, px + xx, py + yy, palette_color(zvb, color));
                    }
                }
            } else if (tile < 256) {
                /* 8-bit mode: only the first 256 tiles exist */
                for (int yy = 0; yy < TILE_H; yy++) {
                    for (int xx = 0; xx < TILE_W; xx++) {
                        const int offset = yy * TILE_W + xx;
                        img_put_pixel(img, px + xx, py + yy,
                                      palette_color(zvb, zvb->tileset.raw[tile * 256 + offset]));
                    }
                }
            }
            /* 8-bit mode, tile >= 256: leave the cell transparent */
        }
    }
    UpdateTexture(zvb->debug_tex[DBG_TILESET], img->data);
}

/* ------------------------------------------------------------------ */
/*  Layer 0 / Layer 1 (text or graphics mode)                          */
/* ------------------------------------------------------------------ */

static void render_layer_cell_gfx(zvb_t* zvb, Image* img, bool layer1, int index, int px, int py)
{
    const int color_4bit = zvb->mode == MODE_GFX_640_4BIT || zvb->mode == MODE_GFX_320_4BIT;
    const uint8_t l0_idx = zvb->layers.raw_layer0[index];

    for (int yy = 0; yy < TILE_H; yy++) {
        for (int xx = 0; xx < TILE_W; xx++) {
            Color color;
            if (color_4bit) {
                /* 4-bit mode: layer1 is an attribute byte (bit0=+256 tiles,
                 * bit2=flip Y, bit3=flip X, high nibble=palette offset) */
                const uint8_t attr = zvb->layers.raw_layer1[index];
                const int l0 = l0_idx + ((attr & 0x01) ? 256 : 0);
                const int ox = (attr & 0x08) ? (TILE_W - 1) - xx : xx;
                const int oy = (attr & 0x04) ? (TILE_H - 1) - yy : yy;
                const int final_idx = l0 * 256 + oy * TILE_W + ox;
                const uint8_t byte = zvb->tileset.raw[final_idx / 2];
                const int col = (final_idx & 1) ? (byte & 0x0f) : (byte >> 4);
                color = palette_color(zvb, col + (attr & 0xf0));
            } else if (!layer1) {
                const int offset = yy * TILE_W + xx;
                color = palette_color(zvb, zvb->tileset.raw[l0_idx * 256 + offset]);
            } else {
                /* Layer 1: transparent pixels (color index 0) show a grey sub-grid */
                const int offset = yy * TILE_W + xx;
                const uint8_t l1_col = zvb->tileset.raw[zvb->layers.raw_layer1[index] * 256 + offset];
                if (l1_col == 0) {
                    const int sub_idx = (yy / 8) * 2 + (xx / 8);
                    color = (sub_idx == 0 || sub_idx == 3) ? GREY_DARK : GREY_LIGHT;
                } else {
                    color = palette_color(zvb, l1_col);
                }
            }
            img_put_pixel(img, px + xx, py + yy, color);
        }
    }
}

static void render_layer_cell_text(zvb_t* zvb, Image* img, bool layer1, int index, int px, int py)
{
    /* Text mode: layers hold characters and their attributes */
    const uint8_t attr = zvb->layers.raw_layer1[index];
    const Color fg = palette_color(zvb, attr & 0x0f);
    const Color bg = palette_color(zvb, (attr >> 4) & 0x0f);

    if (layer1) {
        /* left half = background color, right half = foreground color */
        for (int yy = 0; yy < CHAR_H; yy++) {
            for (int xx = 0; xx < CHAR_W; xx++) {
                img_put_pixel(img, px + xx, py + yy, xx < CHAR_W / 2 ? bg : fg);
            }
        }
    } else {
        const uint8_t character = zvb->layers.raw_layer0[index];
        const uint8_t* raw = &zvb->font.raw_font[character * CHAR_H];
        for (int yy = 0; yy < CHAR_H; yy++) {
            uint8_t bits = raw[yy];
            for (int xx = 0; xx < CHAR_W; xx++) {
                const Color c = (bits & 0x80) ? fg : bg;
                img_put_pixel(img, px + xx, py + yy, c);
                bits <<= 1;
            }
        }
    }
}

static void render_layer(zvb_t* zvb, bool layer1)
{
    const bool gfx_mode = zvb_is_gfx_mode(zvb);
    const dbg_vram_t view = layer1 ? DBG_TILEMAP_LAYER1 : DBG_TILEMAP_LAYER0;
    const dbg_grid_t g = debug_grid(view, gfx_mode);
    Image* img = &zvb->debug_img[view];

    img_fill(img, BLANK);
    debug_draw_grid(img, &g, gfx_mode);
    if (gfx_mode) {
        debug_draw_subscreens(img, &g);
    }

    for (int cy = 0; cy < g.rows; cy++) {
        for (int cx = 0; cx < g.columns; cx++) {
            const int index = cy * 80 + cx;
            const int px = cx * g.cell_w + 1;
            const int py = cy * g.cell_h + 1;
            if (gfx_mode) {
                render_layer_cell_gfx(zvb, img, layer1, index, px, py);
            } else {
                render_layer_cell_text(zvb, img, layer1, index, px, py);
            }
        }
    }
    UpdateTexture(zvb->debug_tex[view], img->data);
}

void zvb_render_debug_textures(zvb_t* zvb, dbg_vram_t view)
{
    switch (view) {
        case DBG_TILEMAP_LAYER0:
            render_layer(zvb, false);
            break;
        case DBG_TILEMAP_LAYER1:
            render_layer(zvb, true);
            break;
        case DBG_TILESET:
            render_tileset(zvb);
            break;
        case DBG_PALETTE:
            render_palette(zvb);
            break;
        case DBG_FONT:
            render_font(zvb);
            break;
        default:
            break;
    }
}

#endif
