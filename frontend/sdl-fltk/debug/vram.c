/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */

/* VRAM debugger views rasterised into plain RGB buffers for FLTK images.
 * Ported from the Nuklear frontend's Raylib image renderer. */

#include <stdlib.h>
#include "frontend/sdl-fltk/frontend.h"

#define CHAR_W 8
#define CHAR_H 12
#define TILE_W 16
#define TILE_H 16

typedef struct {
    uint8_t r, g, b;
} dbg_color_t;

typedef struct {
    dbg_color_t* data;
    int width;
    int height;
} dbg_img_t;

#define GRID_COLOR      ((dbg_color_t){ 166, 0, 0 })
#define SUBSCREEN_COLOR ((dbg_color_t){ 0, 165, 0 })
#define GREY_LIGHT      ((dbg_color_t){ 204, 204, 204 })
#define GREY_DARK       ((dbg_color_t){ 127, 127, 127 })
#define EMPTY_COLOR     ((dbg_color_t){ 32, 32, 36 })
#define BLACK_COLOR     ((dbg_color_t){ 0, 0, 0 })
#define WHITE_COLOR     ((dbg_color_t){ 255, 255, 255 })

static const int s_widths[DBG_VIEW_TOTAL]  = { ZVB_DBG_RES_WIDTH, ZVB_DBG_RES_WIDTH, 273, 273, 145 };
static const int s_heights[DBG_VIEW_TOTAL] = { ZVB_DBG_RES_HEIGHT, ZVB_DBG_RES_HEIGHT, 545, 273, 209 };
static dbg_img_t s_images[DBG_VIEW_TOTAL];

static dbg_color_t palette_color(const zvb_t* zvb, int index)
{
    const uint8_t* raw = &zvb->palette.raw_palette[index * 2];
    const uint16_t rgb = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
    return (dbg_color_t) {
        (uint8_t)((((rgb >> 11) & 31) * 255) / 31),
        (uint8_t)((((rgb >> 5) & 63) * 255) / 63),
        (uint8_t)(((rgb & 31) * 255) / 31),
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
static inline void img_put_pixel(dbg_img_t* img, int x, int y, dbg_color_t color)
{
    img->data[y * img->width + x] = color;
}

/* Fill the whole image buffer with one color. */
static inline void img_fill(dbg_img_t* img, dbg_color_t color)
{
    dbg_color_t* data = img->data;
    const int count = img->width * img->height;
    for (int i = 0; i < count; i++) {
        data[i] = color;
    }
}

/* Fill a rectangle in the image buffer directly. */
static inline void img_fill_rect(dbg_img_t* img, int x, int y, int w, int h, dbg_color_t color)
{
    dbg_color_t* data = img->data;
    for (int j = 0; j < h; j++) {
        dbg_color_t* row = &data[(y + j) * img->width + x];
        for (int i = 0; i < w; i++) {
            row[i] = color;
        }
    }
}

/* Draw the 1px red grid. Views whose grid fills the whole texture (tileset/
 * palette/font, and the layers in graphics mode) also get the right/bottom
 * border line. */
static void debug_draw_grid(dbg_img_t* img, const dbg_grid_t* g, bool gfx_mode)
{
    const bool fills = !g->layer_view || gfx_mode;
    const int x_lines = fills ? g->columns : g->columns - 1;
    const int y_lines = fills ? g->rows : g->rows - 1;
    const int width = x_lines * g->cell_w + 1;
    const int height = y_lines * g->cell_h + 1;
    dbg_color_t* data = img->data;

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
 * green line at every 20th/15th cell. */
static void debug_draw_subscreens(dbg_img_t* img, const dbg_grid_t* g)
{
    dbg_color_t* data = img->data;
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
    dbg_img_t* img = &s_images[DBG_PALETTE];
    const dbg_grid_t g = debug_grid(DBG_PALETTE, false);

    img_fill(img, EMPTY_COLOR);
    debug_draw_grid(img, &g, false);

    for (int y = 0; y < g.rows; y++) {
        for (int x = 0; x < g.columns; x++) {
            img_fill_rect(img, x * g.cell_w + 1, y * g.cell_h + 1,
                          TILE_W, TILE_H, palette_color(zvb, y * 16 + x));
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Font: 256 characters (8x12 bitmaps)                                */
/* ------------------------------------------------------------------ */

static void render_font(zvb_t* zvb)
{
    dbg_img_t* img = &s_images[DBG_FONT];
    const dbg_grid_t g = debug_grid(DBG_FONT, false);

    img_fill(img, BLACK_COLOR);
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
                        img_put_pixel(img, px + cx, py + cy, WHITE_COLOR);
                    }
                    bits <<= 1;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Tileset: 16x32 tiles (16x16 each)                                  */
/* ------------------------------------------------------------------ */

static void render_tileset(zvb_t* zvb)
{
    dbg_img_t* img = &s_images[DBG_TILESET];
    const dbg_grid_t g = debug_grid(DBG_TILESET, false);
    const bool color_4bit = zvb->mode == MODE_GFX_640_4BIT || zvb->mode == MODE_GFX_320_4BIT;

    img_fill(img, EMPTY_COLOR);
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
}

/* ------------------------------------------------------------------ */
/*  Layer 0 / Layer 1 (text or graphics mode)                          */
/* ------------------------------------------------------------------ */

static void render_layer_cell_gfx(zvb_t* zvb, dbg_img_t* img, bool layer1, int index, int px, int py)
{
    const int color_4bit = zvb->mode == MODE_GFX_640_4BIT || zvb->mode == MODE_GFX_320_4BIT;
    const uint8_t l0_idx = zvb->layers.raw_layer0[index];

    for (int yy = 0; yy < TILE_H; yy++) {
        for (int xx = 0; xx < TILE_W; xx++) {
            dbg_color_t color;
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

static void render_layer_cell_text(zvb_t* zvb, dbg_img_t* img, bool layer1, int index, int px, int py)
{
    /* Text mode: layers hold characters and their attributes */
    const uint8_t attr = zvb->layers.raw_layer1[index];
    const dbg_color_t fg = palette_color(zvb, attr & 0x0f);
    const dbg_color_t bg = palette_color(zvb, (attr >> 4) & 0x0f);

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
                const dbg_color_t c = (bits & 0x80) ? fg : bg;
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
    dbg_img_t* img = &s_images[view];

    img_fill(img, EMPTY_COLOR);
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
}

bool fltk_debug_video_render(zvb_t* zvb, dbg_vram_t view, fz_image_t* out)
{
    if (view < 0 || view >= DBG_VIEW_TOTAL || out == NULL) return false;
    dbg_img_t* img = &s_images[view];
    if (img->data == NULL) {
        img->data = calloc((size_t) s_widths[view] * s_heights[view], sizeof(dbg_color_t));
        if (img->data == NULL) return false;
        img->width = s_widths[view];
        img->height = s_heights[view];
    }

    switch (view) {
        case DBG_TILEMAP_LAYER0: render_layer(zvb, false); break;
        case DBG_TILEMAP_LAYER1: render_layer(zvb, true);  break;
        case DBG_TILESET:        render_tileset(zvb);      break;
        case DBG_PALETTE:        render_palette(zvb);      break;
        case DBG_FONT:           render_font(zvb);         break;
        default: return false;
    }

    const dbg_grid_t g = debug_grid(view, zvb_is_gfx_mode(zvb));
    out->pixels = (const uint8_t*) img->data;
    out->width = img->width;
    out->height = img->height;
    out->cell_width = g.cell_w - 1;
    out->cell_height = g.cell_h - 1;
    out->columns = g.columns;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Sprites (decoded like zvb_blitter_sprites_scanline)                */
/* ------------------------------------------------------------------ */

#define SPRITE_TILE_BYTES   256
#define SPRITE_HIGH_BANK    256

bool fltk_debug_sprite(zvb_t* zvb, int index, fz_sprite_t* info, uint8_t* rgb, uint8_t* mask)
{
    if (index < 0 || index >= ZVB_SPRITES_COUNT || info == NULL) return false;
    const zvb_sprite_t* sp = &zvb->sprites.data[index];
    const bool color_4bit = zvb->mode == MODE_GFX_640_4BIT || zvb->mode == MODE_GFX_320_4BIT;
    const bool mode_320 = zvb->mode == MODE_GFX_320_8BIT || zvb->mode == MODE_GFX_320_4BIT;
    const int scr_w = mode_320 ? 320 : 640;
    const int scr_h = mode_320 ? 240 : 480;

    info->x = sp->x;
    info->y = sp->y;
    info->screen_x = (int) sp->x - TILE_W;
    info->screen_y = (int) sp->y - TILE_H;
    info->height = sp->extra_flags.bitmap.height_32 ? 32 : 16;
    info->bank = sp->flags.bitmap.tileset_idx;
    info->tile = sp->flags.bitmap.tile_number + ((info->bank && color_4bit) ? SPRITE_HIGH_BANK : 0);
    info->palette = sp->flags.bitmap.palette;
    info->flip_x = sp->flags.bitmap.flip_x;
    info->flip_y = sp->flags.bitmap.flip_y;
    info->behind_fg = sp->flags.bitmap.behind_fg;
    info->raw_flags = sp->flags.raw;
    info->raw_extra = sp->extra_flags.raw;
    info->on_screen = zvb_is_gfx_mode(zvb) &&
        info->screen_x < scr_w && info->screen_x + TILE_W > 0 &&
        info->screen_y < scr_h && info->screen_y + info->height > 0;

    if (rgb == NULL && mask == NULL) return true;
    for (int oy = 0; oy < FZ_SPRITE_MAX_H; oy++) {
        for (int ox = 0; ox < FZ_SPRITE_W; ox++) {
            const int out = oy * FZ_SPRITE_W + ox;
            uint8_t opaque = 0;
            dbg_color_t c = { 0, 0, 0 };
            if (oy < info->height) {
                const int sy = info->flip_y ? info->height - 1 - oy : oy;
                const int sx = info->flip_x ? TILE_W - 1 - ox : ox;
                const int offset = sy * TILE_W + sx;
                int color_idx;
                if (color_4bit) {
                    const int byte_idx = info->tile * SPRITE_TILE_BYTES + offset;
                    const uint8_t byte = zvb->tileset.raw[(byte_idx / 2) & (ZVB_TILESET_SIZE - 1)];
                    color_idx = (byte_idx & 1) ? (byte & 0x0f) : (byte >> 4);
                } else {
                    color_idx = zvb->tileset.raw[(info->tile * SPRITE_TILE_BYTES + offset) & (ZVB_TILESET_SIZE - 1)];
                }
                if (color_idx != 0) {
                    const int final_idx = color_4bit ? ((info->palette << 4) | color_idx) : color_idx;
                    c = palette_color(zvb, final_idx & 0xff);
                    opaque = 1;
                }
            }
            if (rgb) {
                rgb[out * 3 + 0] = c.r;
                rgb[out * 3 + 1] = c.g;
                rgb[out * 3 + 2] = c.b;
            }
            if (mask) mask[out] = opaque;
        }
    }
    return true;
}

void fltk_debug_video_free(void)
{
    for (int i = 0; i < DBG_VIEW_TOTAL; i++) {
        free(s_images[i].data);
        s_images[i].data = NULL;
    }
}
