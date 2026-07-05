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


/** Write an RGB565 pixel into the framebuffer. */
static inline void put_pixel(uint16_t* fb, int x, int y, uint16_t rgb565)
{
    if ((unsigned)x >= FB_WIDTH || (unsigned)y >= FB_HEIGHT) return;
    fb[y * FB_WIDTH + x] = rgb565;
}

/** Return 1 if font bitmap[char_idx][row] has bit col (0=leftmost) set. */
static inline int font_bit(const uint8_t* raw_font, int char_idx,
                           int row, int col)
{
    uint8_t byte = raw_font[char_idx * CHAR_H + row];
    return (byte >> (7 - col)) & 1;
}

/** Get 8-bit colour index from tileset at tile * 256 + offset. */
static inline uint8_t tileset_8bit(const uint8_t* raw, int tile, int offset)
{
    return raw[tile * TILESET_BYTES_PER_TILE + offset];
}

/** Get 4-bit colour index from tileset (2 pixels per byte). */
static inline uint8_t tileset_4bit(const uint8_t* raw, int byte_idx)
{
    uint8_t byte = raw[byte_idx / 2];
    return (byte_idx & 1) ? (byte & 0x0F) : (byte >> 4);
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
    for (int i = 0; i < DBG_VIEW_TOTAL; i++) {
        if (zvb->debug_tex[i].id != 0) {
            UnloadRenderTexture(zvb->debug_tex[i]);
            zvb->debug_tex[i].id = 0;
        }
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

void zvb_blitter_render_text_mode(zvb_t* zvb)
{
    zvb_blitter_t* bl = &zvb->blitter;
    uint16_t* fb = bl->framebuffer;
    const uint8_t* layer0 = zvb->layers.raw_layer0;
    const uint8_t* layer1 = zvb->layers.raw_layer1;
    const uint8_t* font   = zvb->font.raw_font;
    const uint16_t* pal_rgb = zvb_get_palette(zvb);

    bool mode_320 = (zvb->mode == MODE_TEXT_320);
    int scale = mode_320 ? 2 : 1;

    /* Cursor state */
    zvb_text_info_t info;
    zvb_text_update(&zvb->text, &info);
    const int cur_x = info.pos[0];
    const int cur_y = info.pos[1];
    const int cur_bg = info.color[0];
    const int cur_fg = info.color[1];
    const int cur_ch = info.charidx;
    const int scroll_x = info.scroll[0];
    const int scroll_y = info.scroll[1];

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {

            int eff_col = (col + scroll_x) % COLS;
            int eff_row = (row + scroll_y) % ROWS;

            uint8_t tile  = layer0[eff_col + eff_row * COLS];
            uint8_t attr  = layer1[eff_col + eff_row * COLS];
            uint8_t fg_idx = attr & 0x0F;
            uint8_t bg_idx = (attr >> 4) & 0x0F;

            /* Cursor override */
            if (col == cur_x && row == cur_y) {
                tile   = (uint8_t)(cur_ch & 0xFF);
                bg_idx = (uint8_t)(cur_bg & 0x0F);
                fg_idx = (uint8_t)(cur_fg & 0x0F);
            }

            uint16_t fg_rgb = pal_rgb[fg_idx];
            uint16_t bg_rgb = pal_rgb[bg_idx];

            for (int cy = 0; cy < CHAR_H; cy++) {
                for (int cx = 0; cx < CHAR_W; cx++) {
                    int on = font_bit(font, tile, cy, cx);
                    uint16_t color = on ? fg_rgb : bg_rgb;
                    int px = col * CHAR_W + cx;
                    int py = row * CHAR_H + cy;
                    if (scale == 2) {
                        for (int sy = 0; sy < 2; sy++) {
                            for (int sx = 0; sx < 2; sx++) {
                                put_pixel(fb, px * 2 + sx, py * 2 + sy, color);
                            }
                        }
                    } else {
                        put_pixel(fb, px, py, color);
                    }
                }
            }
        }
    }

    UpdateTexture(bl->output_texture, bl->framebuffer);
    BeginTextureMode(bl->main_texture);
        DrawTextureRec(bl->output_texture, (Rectangle){ 0, 0, FB_WIDTH, -FB_HEIGHT }, (Vector2){ 0, 0 }, WHITE);
    EndTextureMode();
}

void zvb_blitter_render_debug_text_mode(zvb_t* zvb)
{
    (void)zvb;
}

/* ================================================================== */
/*  BITMAP MODE                                                        */
/* ================================================================== */

static void zvb_blitter_scale_render(zvb_t* zvb)
{
    bool scale2x = (zvb->mode == MODE_GFX_320_8BIT || zvb->mode == MODE_GFX_320_4BIT);

#if ZVB_BLITTER_SOFTWARE_SCALING
        if (scale2x) {
            uint16_t* fb = zvb->blitter.framebuffer;
            for (int y = 239; y >= 0; y--) {
                for (int x = 319; x >= 0; x--) {
                    uint16_t c = fb[y * FB_WIDTH + x];
                    int dy = y * 2;
                    int dx = x * 2;
                    fb[(dy+1) * FB_WIDTH + (dx+1)] = c;
                    fb[(dy+1) * FB_WIDTH + dx]     = c;
                    fb[dy * FB_WIDTH     + (dx+1)] = c;
                    fb[dy * FB_WIDTH     + dx]     = c;
                }
            }
        }

        zvb_blitter_t* bl = &zvb->blitter;
        UpdateTexture(bl->output_texture, bl->framebuffer);
        BeginTextureMode(bl->main_texture);
            DrawTextureRec(bl->output_texture,
                (Rectangle){ 0, 0, FB_WIDTH, -FB_HEIGHT },
                (Vector2){ 0, 0 }, WHITE);
        EndTextureMode();
#else
        int src_w = scale2x ? 320 : 640;
        int src_h = scale2x ? 240 : 480;

        zvb_blitter_t* bl = &zvb->blitter;
        UpdateTexture(bl->output_texture, bl->framebuffer);
        BeginTextureMode(bl->main_texture);
            DrawTexturePro(bl->output_texture,
                (Rectangle){ 0, 0, src_w, -src_h },
                (Rectangle){ 0, 0, FB_WIDTH, FB_HEIGHT },
                (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndTextureMode();
#endif
}


void zvb_blitter_prepare_render_bitmap_mode(zvb_t* zvb)
{
    (void)zvb;
}

static inline void bitmap_put_pixel(uint16_t* fb, int x, int y, uint16_t color)
{
    put_pixel(fb, 2 * x, y, color);
    put_pixel(fb, 2 * x + 1, y, color);
}

static void zvb_blitter_render_bitmap_scanline(zvb_t* zvb, int scanline)
{
    uint16_t* fb = zvb->blitter.framebuffer;
    const uint8_t* vram = zvb->tileset.raw;
    const uint16_t* pal_rgb = zvb_get_palette(zvb);
    const uint8_t border_idx = vram[0xFFFF];
    const uint16_t border_rgb = pal_rgb[border_idx];
    /* Bitmap modes on real hardware run in 320x240 resolution */
    int virt_scanline = scanline / 2;

    if (zvb->mode == MODE_BITMAP_256) {
        /* In 256x240 mode, we have a left and right border of 32 pixels (256 + 2*32 = 320px) */
        const int border = 32;
        for (int i = 0; i < border; i++) {
            /* Left border */
            bitmap_put_pixel(fb, i, scanline, border_rgb);
        }
        for (int x = 0; x < 256; x++) {
            const int by = virt_scanline;
            uint16_t index = (uint16_t)(by * 256 + x);
            uint16_t color = pal_rgb[vram[index]];
            bitmap_put_pixel(fb, border + x, scanline, color);
        }
        for (int i = 0; i < border; i++) {
            /* Right border */
            bitmap_put_pixel(fb, border + 256 + i, scanline, border_rgb);
        }
    } else {
        /* In 320x200, we have a top and bottom border of 20 pixels (200 + 2*20 = 240px) */
        const int border = 20;
        if (virt_scanline < border || virt_scanline >= border + 200) {
            for (int x = 0; x < FB_WIDTH / 2; x++) {
                bitmap_put_pixel(fb, x, scanline, border_rgb);
            }
        } else {
            for (int x = 0; x < FB_WIDTH / 2; x++) {
                const int by = virt_scanline - border;
                uint16_t index = (uint16_t)(by * 320 + x);
                uint16_t color = pal_rgb[vram[index]];
                bitmap_put_pixel(fb, x, scanline, color);
            }
        }
    }
}

void zvb_blitter_render_bitmap_mode(zvb_t* zvb)
{
#if !ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
    for (int y = 0; y < FB_HEIGHT; y++) {
        zvb_blitter_render_bitmap_scanline(zvb, y);
    }
#endif
    zvb_blitter_scale_render(zvb);
}

/* ================================================================== */
/*  GFX MODE                                                           */
/* ================================================================== */

void zvb_blitter_prepare_render_gfx_mode(zvb_t* zvb)
{
    (void)zvb;
}

static void zvb_blitter_sprites_scanline(zvb_t* zvb, int scanline,
                                         uint16_t* sprites_scanline, uint8_t* sprites_behind_fg,
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
    memset(sprites_behind_fg, 0, scr_w);

    /* Get the list of visible sprites for this scanline */
    uint8_t visible_sprites[ZVB_SPRITES_COUNT];
    int num_visible = zvb_sprites_get_visible_sprites(&zvb->sprites, scanline, visible_sprites);

    for (int si = 0; si < num_visible; si++) {
        uint8_t sprite_idx = visible_sprites[si];
        const zvb_sprite_t* sp = &sprites[sprite_idx];

        int sy = (int)sp->y - TILE_H;
        int sh = sp->extra_flags.bitmap.height_32 ? 32 : 16;
        if (scanline >= sy + sh)
            continue;

        int sx = (int)sp->x - TILE_W;
        int sp_oy = scanline - sy;
        if (sp->flags.bitmap.flip_y) sp_oy = sh - 1 - sp_oy;

        int tile_num = sp->flags.bitmap.tile_number;
        if (sp->flags.bitmap.tileset_idx && !color_4bit)
            tile_num += 256;

        int start_x = sx;
        int end_x = sx + TILE_W;
        if (start_x < 0) start_x = 0;
        if (end_x > scr_w) end_x = scr_w;

        for (int px = start_x; px < end_x; px++) {
            int sp_ox = px - sx;
            if (sp->flags.bitmap.flip_x) sp_ox = (TILE_W - 1) - sp_ox;

            int byte_idx = tile_num * TILESET_BYTES_PER_TILE + sp_oy * TILE_W + sp_ox;
            uint8_t color_idx;
            if (color_4bit) {
                color_idx = tileset_4bit(tileset, byte_idx);
            } else {
                color_idx = tileset_8bit(tileset, tile_num, sp_oy * TILE_W + sp_ox);
            }

            if (color_idx == 0)
                continue;

            int final_idx;
            if (color_4bit) {
                final_idx = sp->flags.bitmap.palette + color_idx;
            } else {
                final_idx = color_idx;
            }

            sprites_scanline[px] = pal_rgb[final_idx & 0xFF];
            sprites_behind_fg[px] = sp->flags.bitmap.behind_fg ? 1 : 0;
        }
    }
}


static void render_gfx_4bit_scanline(zvb_t* zvb, int py,
        uint16_t* sprites_scanline, uint8_t* sprites_behind_fg,
        const uint16_t* pal_rgb)
{
    uint16_t* fb            = zvb->blitter.framebuffer;
    const uint8_t* layer0   = zvb->layers.raw_layer0;
    const uint8_t* layer1   = zvb->layers.raw_layer1;
    const uint8_t* tileset  = zvb->tileset.raw;
    int scr_w = (zvb->mode == MODE_GFX_320_4BIT) ? 320 : 640;
    int scroll_x = (int)zvb->ctrl.l0_scroll_x;
    int scroll_y = (int)zvb->ctrl.l0_scroll_y;

    int l0_py = (py + scroll_y) % 640;
    int l0_ty = l0_py / TILE_H;
    int l0_oy = l0_py % TILE_H;

    (void)sprites_behind_fg;    /* Unused in 4-bit (behind_fg needs opaque layer1) */

    for (int px = 0; px < scr_w; ) {
        int l0_px  = (px + scroll_x) % 1280;
        int l0_tx  = l0_px / TILE_W;
        int l0_ox  = l0_px % TILE_W;
        int burst  = TILE_W - l0_ox;
        if (px + burst > scr_w) burst = scr_w - px;

        uint8_t l0_tile = layer0[l0_tx + l0_ty * COLS];
        int attr        = layer1[l0_tx + l0_ty * COLS];
        int tile_idx    = l0_tile;
        if (attr & 1) tile_idx += 256;

        int oy       = (attr & 4) ? (TILE_H - 1) - l0_oy : l0_oy;
        int row_base = tile_idx * TILESET_BYTES_PER_TILE + oy * TILE_W;

        for (int i = 0; i < burst; i++, px++) {
            int ox = l0_ox + i;
            if (attr & 8) ox = (TILE_W - 1) - ox;

            uint8_t nibble    = tileset_4bit(tileset, row_base + ox);
            uint8_t color_idx = (uint8_t)((attr & 0xF0) + nibble);
            uint16_t pixel_rgb = pal_rgb[color_idx];

            uint16_t sp = sprites_scanline[px];
            if (sp != 0) pixel_rgb = sp;

            put_pixel(fb, px, py, pixel_rgb);
        }
    }
}

static void render_gfx_8bit_scanline(zvb_t* zvb, int py,
        uint16_t* sprites_scanline, uint8_t* sprites_behind_fg,
        const uint16_t* pal_rgb)
{
    uint16_t* fb            = zvb->blitter.framebuffer;
    const uint8_t* layer0   = zvb->layers.raw_layer0;
    const uint8_t* layer1   = zvb->layers.raw_layer1;
    const uint8_t* tileset  = zvb->tileset.raw;
    int scr_w = (zvb->mode == MODE_GFX_320_8BIT) ? 320 : 640;
    int scroll_l0_x = (int)zvb->ctrl.l0_scroll_x;
    int scroll_l0_y = (int)zvb->ctrl.l0_scroll_y;
    int scroll_l1_x = (int)zvb->ctrl.l1_scroll_x;
    int scroll_l1_y = (int)zvb->ctrl.l1_scroll_y;

    int l0_py = (py + scroll_l0_y) % 640;
    int l1_py = (py + scroll_l1_y) % 640;
    int l0_ty = l0_py / TILE_H;
    int l1_ty = l1_py / TILE_H;
    int l0_oy = l0_py % TILE_H;
    int l1_oy = l1_py % TILE_H;

    for (int px = 0; px < scr_w; ) {
        int l0_px  = (px + scroll_l0_x) % 1280;
        int l0_tx  = l0_px / TILE_W;
        int l0_ox  = l0_px % TILE_W;
        int burst  = TILE_W - l0_ox;
        if (px + burst > scr_w) {
            burst = scr_w - px;
        }

        int l1_px  = (px + scroll_l1_x) % 1280;
        int l1_tx  = l1_px / TILE_W;
        int l1_ox  = l1_px % TILE_W;
        int l1_rem = TILE_W - l1_ox;
        if (l1_rem < burst) burst = l1_rem;

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
            if (sp != 0 && !(opaque && sprites_behind_fg[px]))
                pixel_rgb = sp;

            put_pixel(fb, px, py, pixel_rgb);
        }
    }
}


#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING

void zvb_blitter_render_scanline(zvb_t* zvb, int scanline)
{
    uint16_t* pal_rgb = zvb_get_palette(zvb);

    if (zvb->mode < MODE_GFX_640_8BIT) {
        /* Text / Bitmap modes */
        if (zvb_is_text_mode(zvb)) return; /* TODO: text scanline */
        zvb_blitter_render_bitmap_scanline(zvb, scanline);
        return;
    }

    uint16_t  sprites_scanline[FB_WIDTH];
    uint8_t   sprites_behind_fg[FB_WIDTH];

    /* In case we are in 320x240 mode, we need to divide the scanline by 2 */
    if (zvb->mode == MODE_GFX_320_4BIT || zvb->mode == MODE_GFX_320_8BIT) {
        scanline /= 2;
    }

    zvb_blitter_sprites_scanline(zvb, scanline, sprites_scanline, sprites_behind_fg, pal_rgb);

    if (zvb->mode == MODE_GFX_640_4BIT || zvb->mode == MODE_GFX_320_4BIT) {
        render_gfx_4bit_scanline(zvb, scanline, sprites_scanline, sprites_behind_fg, pal_rgb);
    } else {
        render_gfx_8bit_scanline(zvb, scanline, sprites_scanline, sprites_behind_fg, pal_rgb);
    }
}

void zvb_blitter_render_gfx_mode(zvb_t* zvb)
{
    zvb_blitter_scale_render(zvb);
}

#else /* !ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING */

void zvb_blitter_render_scanline(zvb_t* zvb, int scanline)
{
    /* Full-frame software rendering does not render during scanline ticks. */
    (void)zvb;
    (void)scanline;
}

static void render_gfx_4bit(zvb_t* zvb, uint16_t* sprites_scanline,
                         uint8_t* sprites_behind_fg, const uint16_t* pal_rgb)
{
    int scr_h = (zvb->mode == MODE_GFX_320_4BIT) ? 240 : 480;

    for (int py = 0; py < scr_h; py++) {
        zvb_blitter_sprites_scanline(zvb, py, sprites_scanline, sprites_behind_fg, pal_rgb);
        render_gfx_4bit_scanline(zvb, py, sprites_scanline, sprites_behind_fg, pal_rgb);
    }
}

static void render_gfx_8bit(zvb_t* zvb, uint16_t* sprites_scanline,
                         uint8_t* sprites_behind_fg, const uint16_t* pal_rgb)
{
    int scr_h = (zvb->mode == MODE_GFX_320_8BIT) ? 240 : 480;

    for (int py = 0; py < scr_h; py++) {
        zvb_blitter_sprites_scanline(zvb, py, sprites_scanline, sprites_behind_fg, pal_rgb);
        render_gfx_8bit_scanline(zvb, py, sprites_scanline, sprites_behind_fg, pal_rgb);
    }
}

void zvb_blitter_render_gfx_mode(zvb_t* zvb)
{
    uint16_t sprites_scanline[FB_WIDTH];
    uint8_t sprites_behind_fg[FB_WIDTH];

    /* Pre-compute palette as RGB565 for fast lookup */
    uint16_t* pal_rgb = zvb_get_palette(zvb);

    if (zvb->mode == MODE_GFX_640_4BIT || zvb->mode == MODE_GFX_320_4BIT) {
        render_gfx_4bit(zvb, sprites_scanline, sprites_behind_fg, pal_rgb);
    } else {
        render_gfx_8bit(zvb, sprites_scanline, sprites_behind_fg, pal_rgb);
    }

    zvb_blitter_scale_render(zvb);
}

#endif /* ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING */

void zvb_blitter_render_debug_gfx_mode(zvb_t* zvb)
{
    (void)zvb;
}
