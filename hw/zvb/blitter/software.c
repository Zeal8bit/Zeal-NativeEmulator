/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


/* Software blitter: renders all video modes on CPU */
#include "hw/zvb/zvb.h"
#include "hw/zvb/blitter/software.h"
#include "utils/log.h"
#include <stdlib.h>
#include <string.h>

#define BITMAP_256_WIDTH                 (256)
#define BITMAP_256_BORDER                (32)
#define BITMAP_256_END                   (288)
#define BITMAP_320_BORDER                (20)
#define BITMAP_320_END                   (220)
#define LOWRES_WIDTH                     (320)
#define LOWRES_HEIGHT                    (240)
#define BITMAP_BORDER_ADDR               (0xffff)
#define TEXT_COLOR_MASK                  (15)
#define TEXT_COLOR_SHIFT                 (4)
#define SPRITE_PALETTE_SHIFT             (4)
#define FONT_LEFT_PIXEL                  (0x80)
#define LOWRES_SCALE                     (2)
#define TILEMAP_PIXEL_WIDTH              (COLS * TILE_W)
#define TILEMAP_PIXEL_HEIGHT             (ROWS * TILE_H)
#define TILE_HIGH_BANK                   (256)
#define TILE_ATTR_HIGH_BANK              (1)
#define TILE_ATTR_FLIP_Y                 (4)
#define TILE_ATTR_FLIP_X                 (8)
#define TILE_ATTR_PALETTE                (0xf0)
#define COLOR_INDEX_MASK                 (0xff)
#define TILE_COLOR_MASK                  (0x0f)

#define FB_WIDTH    ZVB_MAX_RES_WIDTH
#define FB_HEIGHT   ZVB_MAX_RES_HEIGHT
#define FB_BYTES    (FB_WIDTH * FB_HEIGHT * 2)   /* RGB565 */

#define CHAR_W      8
#define CHAR_H      12
#define COLS        80
#define ROWS        40

#define TILE_W      16
#define TILE_H      16
#define TILESET_BYTES_PER_TILE  256
#define SPRITE_COUNT            ZVB_SPRITES_COUNT
#define SPRITE_OPAQUE           1u
#define SPRITE_BEHIND_FG        2u

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

static uint16_t* zvb_get_palette(zvb_t* zvb)
{
    return (uint16_t*)zvb->palette.raw_palette;
}

#else /* Big endian */

static uint16_t* zvb_get_palette(zvb_t* zvb)
{
    const uint8_t* pal = zvb->palette.raw_palette;
    static uint16_t pal_rgb[256];
    for (int i = 0; i < 256; i++) {
        pal_rgb[i] = pal_rgb565(pal, i);
    }
    return pal_rgb;
}
#endif


/** Get 8-bit colour index from tileset at tile * 256 + offset. */
static inline uint8_t tileset_8bit(const uint8_t* raw, int tile, int offset)
{
    return raw[(tile * TILESET_BYTES_PER_TILE + offset) & (ZVB_TILESET_SIZE - 1)];
}

/** Get 4-bit colour index from tileset (2 pixels per byte). */
static inline uint8_t tileset_4bit(const uint8_t* raw, int byte_idx)
{
    uint8_t byte = raw[(byte_idx / 2) & (ZVB_TILESET_SIZE - 1)];
    return (byte_idx & 1) ? (byte & TILE_COLOR_MASK) : (byte >> 4);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

void zvb_blitter_init(zvb_t* zvb)
{
    zvb_blitter_t* bl = &zvb->blitter;

    v_log_printf(1, "[RENDER] Blitter: software\n");

    bl->framebuffer = (uint16_t*)malloc(FB_BYTES);
    if (!bl->framebuffer) {
        log_err_printf("Software blitter: framebuffer allocation failed\n");
        return;
    }
    memset(bl->framebuffer, 0, FB_BYTES);

    bl->fb_image = (Image){
        .data    = bl->framebuffer,
        .width   = FB_WIDTH,
        .height  = FB_HEIGHT,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R5G6B5,
    };

    bl->output_texture = LoadTextureFromImage(bl->fb_image);
    bl->main_texture   = LoadRenderTexture(FB_WIDTH, FB_HEIGHT);
}

void zvb_blitter_deinit(zvb_t* zvb)
{
    zvb_blitter_t* bl = &zvb->blitter;
    if (bl->output_texture.id != 0) {
        UnloadTexture(bl->output_texture);
        bl->output_texture.id = 0;
    }
    if (bl->main_texture.id != 0) {
        UnloadRenderTexture(bl->main_texture);
        bl->main_texture.id = 0;
    }
    free(bl->framebuffer);
    bl->framebuffer = NULL;

    if (bl->main_texture.id != 0) {
        UnloadRenderTexture(bl->main_texture);
        bl->main_texture.id = 0;
    }
}

/* ================================================================== */
/*  TEXT MODE                                                          */
/* ================================================================== */

void zvb_blitter_prepare_render_text_mode(zvb_t* zvb)
{
    (void)zvb;
    /* Nothing to prepare — we read raw arrays directly. */
}

static bool low_resolution(const zvb_t* zvb)
{
    return zvb->mode == MODE_TEXT_320 || zvb_is_bitmap_mode(zvb) ||
           zvb->mode == MODE_GFX_320_8BIT || zvb->mode == MODE_GFX_320_4BIT;
}

static void render_text_scanline(zvb_t* zvb, int y)
{
    uint16_t* output = zvb->blitter.framebuffer + y * FB_WIDTH;
    const uint16_t* pal = zvb_get_palette(zvb);
    const int columns = low_resolution(zvb) ? LOWRES_WIDTH / CHAR_W : COLS;
    const int font_row = y % CHAR_H;
    zvb_text_info_t info;
    const bool cursor_shown = zvb_text_get_info(&zvb->text, &info);
    const uint32_t cursor_address = zvb_text_cursor_address(&zvb->text);
    const int row_base = ((y / CHAR_H + info.scroll[1]) % ROWS) * COLS;
    for (int col = 0; col < columns; col++) {
        const int index = row_base + (col + info.scroll[0]) % COLS;
        uint8_t tile = zvb->layers.raw_layer0[index];
        uint8_t attr = zvb->layers.raw_layer1[index];
        if (cursor_shown && (uint32_t)index == cursor_address) {
            tile = info.charidx;
            attr = (info.color[0] << TEXT_COLOR_SHIFT) | info.color[1];
        }
        const uint16_t foreground = pal[attr & TEXT_COLOR_MASK];
        const uint16_t background = pal[attr >> TEXT_COLOR_SHIFT];
        uint8_t bits = zvb->font.raw_font[tile * CHAR_H + font_row];
        for (int x = 0; x < CHAR_W; x++) {
            *output++ = (bits & FONT_LEFT_PIXEL) ? foreground : background;
            bits <<= 1;
        }
    }
}

static void present_frame(zvb_t* zvb)
{
    zvb_blitter_t* bl = &zvb->blitter;
    const bool lowres = low_resolution(zvb);
    int width = lowres ? LOWRES_WIDTH : FB_WIDTH;
    int height = lowres ? LOWRES_HEIGHT : FB_HEIGHT;
#if ZVB_BLITTER_SOFTWARE_SCALING
    /* Optional CPU fallback belongs to presentation, not row rendering. */
    if (lowres) {
        for (int y = LOWRES_HEIGHT - 1; y >= 0; y--) {
            for (int x = LOWRES_WIDTH - 1; x >= 0; x--) {
                const uint16_t pixel = bl->framebuffer[y * FB_WIDTH + x];
                const int offset = y * LOWRES_SCALE * FB_WIDTH + x * LOWRES_SCALE;
                bl->framebuffer[offset] = pixel;
                bl->framebuffer[offset + 1] = pixel;
                bl->framebuffer[offset + FB_WIDTH] = pixel;
                bl->framebuffer[offset + FB_WIDTH + 1] = pixel;
            }
        }
        width = FB_WIDTH;
        height = FB_HEIGHT;
    }
#endif
    UpdateTexture(bl->output_texture, bl->framebuffer);
    BeginTextureMode(bl->main_texture);
        DrawTexturePro(bl->output_texture,
            (Rectangle){ 0, 0, width, -height },
            (Rectangle){ 0, 0, FB_WIDTH, FB_HEIGHT },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndTextureMode();
#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
    bl->raster_frame = false;
#endif
}

/* ================================================================== */
/*  BITMAP MODE                                                        */
/* ================================================================== */

void zvb_blitter_prepare_render_bitmap_mode(zvb_t* zvb)
{
    (void)zvb;
}

static void render_bitmap_scanline(zvb_t* zvb, int y)
{
    uint16_t* output = zvb->blitter.framebuffer + y * FB_WIDTH;
    const uint8_t* vram = zvb->tileset.raw;
    const uint16_t* pal = zvb_get_palette(zvb);
    const uint16_t border = pal[vram[BITMAP_BORDER_ADDR]];
    if (zvb->mode == MODE_BITMAP_256) {
        for (int x = 0; x < BITMAP_256_BORDER; x++) {
            output[x] = border;
        }
        const uint8_t* row = vram + y * BITMAP_256_WIDTH;
        for (int x = 0; x < BITMAP_256_WIDTH; x++) {
            output[BITMAP_256_BORDER + x] = pal[row[x]];
        }
        for (int x = BITMAP_256_END; x < LOWRES_WIDTH; x++) {
            output[x] = border;
        }
    } else if (y >= BITMAP_320_BORDER && y < BITMAP_320_END) {
        const uint8_t* row = vram + (y - BITMAP_320_BORDER) * LOWRES_WIDTH;
        for (int x = 0; x < LOWRES_WIDTH; x++) {
            output[x] = pal[row[x]];
        }
    } else {
        for (int x = 0; x < LOWRES_WIDTH; x++) {
            output[x] = border;
        }
    }
}

/* ================================================================== */
/*  GFX MODE                                                           */
/* ================================================================== */

void zvb_blitter_prepare_render_gfx_mode(zvb_t* zvb)
{
    (void)zvb;
}

static void zvb_blitter_sprites_scanline(zvb_t* zvb, int scanline,
                                         uint16_t* sprites_scanline, uint8_t* sprites_flags,
                                         const uint16_t* pal_rgb)
{
    const zvb_sprite_t* sprites = zvb->sprites.data;
    const uint8_t* tileset = zvb->tileset.raw;
    bool color_4bit = (zvb->mode == MODE_GFX_640_4BIT ||
                       zvb->mode == MODE_GFX_320_4BIT);
    bool mode_320   = (zvb->mode == MODE_GFX_320_8BIT ||
                       zvb->mode == MODE_GFX_320_4BIT);
    int scr_w = mode_320 ? 320 : 640;

    memset(sprites_scanline, 0, scr_w * sizeof(uint16_t));
    memset(sprites_flags, 0, scr_w);

    /* Get the list of visible sprites for this scanline */
    uint8_t visible_sprites[ZVB_SPRITES_COUNT];
    int num_visible = zvb_sprites_get_visible_sprites(&zvb->sprites, scanline, visible_sprites);

    for (int si = 0; si < num_visible; si++) {
        uint8_t sprite_idx = visible_sprites[si];
        const zvb_sprite_t* sp = &sprites[sprite_idx];

        int sy = (int)sp->y - TILE_H;
        int sh = sp->extra_flags.bitmap.height_32 ? 32 : 16;
        if (scanline >= sy + sh) {
            continue;
        }

        int sx = (int)sp->x - TILE_W;
        int sp_oy = scanline - sy;
        if (sp->flags.bitmap.flip_y) {
            sp_oy = sh - 1 - sp_oy;
        }

        int tile_num = sp->flags.bitmap.tile_number;
        if (sp->flags.bitmap.tileset_idx && color_4bit) {
            tile_num += TILE_HIGH_BANK;
        }

        int start_x = sx;
        int end_x = sx + TILE_W;
        if (start_x < 0) {
            start_x = 0;
        }
        if (end_x > scr_w) {
            end_x = scr_w;
        }

        for (int px = start_x; px < end_x; px++) {
            int sp_ox = px - sx;
            if (sp->flags.bitmap.flip_x) {
                sp_ox = (TILE_W - 1) - sp_ox;
            }

            int byte_idx = tile_num * TILESET_BYTES_PER_TILE + sp_oy * TILE_W + sp_ox;
            uint8_t color_idx;
            if (color_4bit) {
                color_idx = tileset_4bit(tileset, byte_idx);
            } else {
                color_idx = tileset_8bit(tileset, tile_num, sp_oy * TILE_W + sp_ox);
            }

            if (color_idx == 0) {
                continue;
            }

            int final_idx;
            if (color_4bit) {
                final_idx = (sp->flags.bitmap.palette << SPRITE_PALETTE_SHIFT) | color_idx;
            } else {
                final_idx = color_idx;
            }

            sprites_scanline[px] = pal_rgb[final_idx & COLOR_INDEX_MASK];
            sprites_flags[px] = SPRITE_OPAQUE |
                (sp->flags.bitmap.behind_fg ? SPRITE_BEHIND_FG : 0);
        }
    }
}


static void render_gfx_4bit_scanline(zvb_t* zvb, int py,
        uint16_t* sprites_scanline, uint8_t* sprites_flags,
        const uint16_t* pal_rgb, int width)
{
    uint16_t* output        = zvb->blitter.framebuffer + py * FB_WIDTH;
    const uint8_t* layer0   = zvb->layers.raw_layer0;
    const uint8_t* layer1   = zvb->layers.raw_layer1;
    const uint8_t* tileset  = zvb->tileset.raw;
    int scroll_x = (int)zvb->ctrl.l0_scroll_x;
    int scroll_y = (int)zvb->ctrl.l0_scroll_y;

    int l0_py = (py + scroll_y) % TILEMAP_PIXEL_HEIGHT;
    int l0_ty = l0_py / TILE_H;
    int l0_oy = l0_py % TILE_H;

    for (int px = 0; px < width; ) {
        int l0_px  = (px + scroll_x) % TILEMAP_PIXEL_WIDTH;
        int l0_tx  = l0_px / TILE_W;
        int l0_ox  = l0_px % TILE_W;
        int burst  = TILE_W - l0_ox;
        if (px + burst > width) {
            burst = width - px;
        }

        uint8_t l0_tile = layer0[l0_tx + l0_ty * COLS];
        int attr        = layer1[l0_tx + l0_ty * COLS];
        int tile_idx    = l0_tile;
        if (attr & TILE_ATTR_HIGH_BANK) {
            tile_idx += TILE_HIGH_BANK;
        }

        int oy       = (attr & TILE_ATTR_FLIP_Y) ? (TILE_H - 1) - l0_oy : l0_oy;
        int row_base = tile_idx * TILESET_BYTES_PER_TILE + oy * TILE_W;

        for (int i = 0; i < burst; i++, px++) {
            int ox = l0_ox + i;
            if (attr & TILE_ATTR_FLIP_X) {
                ox = (TILE_W - 1) - ox;
            }

            uint8_t nibble    = tileset_4bit(tileset, row_base + ox);
            uint8_t color_idx = (uint8_t)((attr & TILE_ATTR_PALETTE) + nibble);
            uint16_t pixel_rgb = pal_rgb[color_idx];

            uint16_t sp = sprites_scanline[px];
            if (sprites_flags[px] & SPRITE_OPAQUE) {
                pixel_rgb = sp;
            }

            output[px] = pixel_rgb;
        }
    }
}

static void render_gfx_8bit_scanline(zvb_t* zvb, int py,
        uint16_t* sprites_scanline, uint8_t* sprites_flags,
        const uint16_t* pal_rgb, int width)
{
    uint16_t* output        = zvb->blitter.framebuffer + py * FB_WIDTH;
    const uint8_t* layer0   = zvb->layers.raw_layer0;
    const uint8_t* layer1   = zvb->layers.raw_layer1;
    const uint8_t* tileset  = zvb->tileset.raw;
    int scroll_l0_x = (int)zvb->ctrl.l0_scroll_x;
    int scroll_l0_y = (int)zvb->ctrl.l0_scroll_y;
    int scroll_l1_x = (int)zvb->ctrl.l1_scroll_x;
    int scroll_l1_y = (int)zvb->ctrl.l1_scroll_y;

    int l0_py = (py + scroll_l0_y) % TILEMAP_PIXEL_HEIGHT;
    int l1_py = (py + scroll_l1_y) % TILEMAP_PIXEL_HEIGHT;
    int l0_ty = l0_py / TILE_H;
    int l1_ty = l1_py / TILE_H;
    int l0_oy = l0_py % TILE_H;
    int l1_oy = l1_py % TILE_H;

    for (int px = 0; px < width; ) {
        int l0_px  = (px + scroll_l0_x) % TILEMAP_PIXEL_WIDTH;
        int l0_tx  = l0_px / TILE_W;
        int l0_ox  = l0_px % TILE_W;
        int burst  = TILE_W - l0_ox;
        if (px + burst > width) {
            burst = width - px;
        }

        int l1_px  = (px + scroll_l1_x) % TILEMAP_PIXEL_WIDTH;
        int l1_tx  = l1_px / TILE_W;
        int l1_ox  = l1_px % TILE_W;
        int l1_rem = TILE_W - l1_ox;
        if (l1_rem < burst) {
            burst = l1_rem;
        }

        uint8_t l0_tile = layer0[l0_tx + l0_ty * COLS];
        uint8_t l1_tile = layer1[l1_tx + l1_ty * COLS];

        const uint8_t* l0_row =
            &tileset[l0_tile * TILESET_BYTES_PER_TILE + l0_oy * TILE_W];
        const uint8_t* l1_row =
            &tileset[l1_tile * TILESET_BYTES_PER_TILE + l1_oy * TILE_W];

        for (int i = 0; i < burst; i++, px++) {
            int l0c = l0_row[l0_ox + i];
            int l1c = l1_row[l1_ox + i];

            uint16_t pixel_rgb;
            int opaque = 0;
            if (l1c == 0) {
                pixel_rgb = pal_rgb[l0c];
            } else {
                pixel_rgb = pal_rgb[l1c];
                opaque = 1;
            }

            uint16_t sp = sprites_scanline[px];
            if ((sprites_flags[px] & SPRITE_OPAQUE) &&
                !(opaque && (sprites_flags[px] & SPRITE_BEHIND_FG))) {
                pixel_rgb = sp;
            }

            output[px] = pixel_rgb;
        }
    }
}


static void render_scanline(zvb_t* zvb, int y)
{
    if (zvb_is_text_mode(zvb)) {
        render_text_scanline(zvb, y);
    } else if (zvb_is_bitmap_mode(zvb)) {
        render_bitmap_scanline(zvb, y);
    } else {
        uint16_t sprites[FB_WIDTH];
        uint8_t flags[FB_WIDTH];
        const uint16_t* pal = zvb_get_palette(zvb);
        const int width = low_resolution(zvb) ? LOWRES_WIDTH : FB_WIDTH;
        zvb_blitter_sprites_scanline(zvb, y, sprites, flags, pal);
        if (zvb->mode == MODE_GFX_640_4BIT || zvb->mode == MODE_GFX_320_4BIT) {
            render_gfx_4bit_scanline(zvb, y, sprites, flags, pal, width);
        } else {
            render_gfx_8bit_scanline(zvb, y, sprites, flags, pal, width);
        }
    }
}

#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
void zvb_blitter_render_scanline(zvb_t* zvb, int scanline)
{
    if (!zvb->scanline_rendering || !zvb->blitter.framebuffer ||
        scanline < 0 || scanline >= FB_HEIGHT) {
        return;
    }
    const bool lowres = low_resolution(zvb);
    /* Each low-resolution row is captured once, on its first display line. */
    if (lowres && scanline % LOWRES_SCALE != 0) {
        return;
    }
    const int y = lowres ? scanline / LOWRES_SCALE : scanline;
    const int width = lowres ? LOWRES_WIDTH : FB_WIDTH;
    zvb->blitter.raster_frame = true;
    if (zvb->screen_enabled) {
        render_scanline(zvb, y);
    } else {
        memset(zvb->blitter.framebuffer + y * FB_WIDTH, 0, width * sizeof(uint16_t));
    }
}
#endif

static void render_frame(zvb_t* zvb)
{
#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
    if (!zvb->blitter.raster_frame) {
#endif
        const int height = low_resolution(zvb) ? LOWRES_HEIGHT : FB_HEIGHT;
        for (int y = 0; y < height; y++) {
            render_scanline(zvb, y);
        }
#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
    }
#endif
    present_frame(zvb);
}

void zvb_blitter_render_text_mode(zvb_t* zvb)
{
    render_frame(zvb);
}

void zvb_blitter_render_bitmap_mode(zvb_t* zvb)
{
    render_frame(zvb);
}

void zvb_blitter_render_gfx_mode(zvb_t* zvb)
{
    render_frame(zvb);
}
