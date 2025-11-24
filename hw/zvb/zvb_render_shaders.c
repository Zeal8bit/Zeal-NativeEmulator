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
#include "hw/zvb/zvb_render_shaders.h"
/* Shaders as static strings */
#include "assets/shaders/gfx_shader.h"
#include "assets/shaders/text_shader.h"
#include "assets/shaders/text_debug.h"
#include "assets/shaders/bitmap_shader.h"
#include "assets/shaders/gfx_debug.h"


/**
 * @brief Names of the variables in the shader
 */
#define SHADER_VIDMODE_NAME         "video_mode"
#define SHADER_PALETTE_NAME         "palette"
#define SHADER_FONT_NAME            "font"
#define SHADER_TILEMAPS_NAME        "tilemaps"
#define SHADER_TILESET_NAME         "tileset"
#define SHADER_CURPOS_NAME          "curpos"
#define SHADER_CURCOLOR_NAME        "curcolor"
#define SHADER_CURCHAR_NAME         "curchar"
#define SHADER_TSCROLL_NAME         "scroll"
#define SHADER_SPRITES_NAME         "sprites"
/* Scrolling vlaues for GFX mode */
#define SHADER_SCROLL0_NAME         "scroll_l0"
#define SHADER_SCROLL1_NAME         "scroll_l1"


void zvb_render_init(zvb_render_t* render, zvb_t* dev)
{
    /* Get the indexes of the objects in the shaders */
    zvb_shader_t* st_shader = &render->shaders[SHADER_TEXT];
    log_printf("Compiling shader text_shader\n");
    Shader shader = LoadShaderFromMemory(NULL, s_text_shader);
    st_shader->shader = shader;
    st_shader->objects[TEXT_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[TEXT_SHADER_TILEMAPS_IDX] = GetShaderLocation(shader, SHADER_TILEMAPS_NAME);
    st_shader->objects[TEXT_SHADER_FONT_IDX]     = GetShaderLocation(shader, SHADER_FONT_NAME);
    st_shader->objects[TEXT_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);
    st_shader->objects[TEXT_SHADER_CURPOS_IDX]   = GetShaderLocation(shader, SHADER_CURPOS_NAME);
    st_shader->objects[TEXT_SHADER_CURCOLOR_IDX] = GetShaderLocation(shader, SHADER_CURCOLOR_NAME);
    st_shader->objects[TEXT_SHADER_CURCHAR_IDX]  = GetShaderLocation(shader, SHADER_CURCHAR_NAME);
    st_shader->objects[TEXT_SHADER_TSCROLL_IDX]  = GetShaderLocation(shader, SHADER_TSCROLL_NAME);

    /* Text debug shaders */
    st_shader = &render->shaders[SHADER_TEXT_DEBUG];
    log_printf("Compiling shader text_debug\n");
    shader = LoadShaderFromMemory(NULL, s_text_debug);
    st_shader->shader = shader;
    st_shader->objects[TEXT_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[TEXT_SHADER_TILEMAPS_IDX] = GetShaderLocation(shader, SHADER_TILEMAPS_NAME);
    st_shader->objects[TEXT_SHADER_FONT_IDX]     = GetShaderLocation(shader, SHADER_FONT_NAME);
    st_shader->objects[TEXT_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);
    st_shader->objects[TEXT_SHADER_DBGMODE_IDX]  = GetShaderLocation(shader, "debug_mode");

    st_shader = &render->shaders[SHADER_GFX];
    log_printf("Compiling shader gfx_shader\n");
    shader = LoadShaderFromMemory(NULL, s_gfx_shader);
    st_shader->shader = shader;
    st_shader->objects[GFX_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[GFX_SHADER_TILEMAPS_IDX] = GetShaderLocation(shader, SHADER_TILEMAPS_NAME);
    st_shader->objects[GFX_SHADER_TILESET_IDX]  = GetShaderLocation(shader, SHADER_TILESET_NAME);
    st_shader->objects[GFX_SHADER_SPRITES_IDX]  = GetShaderLocation(shader, SHADER_SPRITES_NAME);
    st_shader->objects[GFX_SHADER_SCROLL0_IDX]  = GetShaderLocation(shader, SHADER_SCROLL0_NAME);
    st_shader->objects[GFX_SHADER_SCROLL1_IDX]  = GetShaderLocation(shader, SHADER_SCROLL1_NAME);
    st_shader->objects[GFX_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);

    st_shader = &render->shaders[SHADER_BITMAP];
    log_printf("Compiling shader bitmap_shader\n");
    shader = LoadShaderFromMemory(NULL, s_bitmap_shader);
    st_shader->shader = shader;
    st_shader->objects[GFX_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[GFX_SHADER_TILESET_IDX]  = GetShaderLocation(shader, SHADER_TILESET_NAME);
    st_shader->objects[GFX_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);

    st_shader = &render->shaders[SHADER_GFX_DEBUG];
    log_printf("Compiling shader gfx_debug\n");
    shader = LoadShaderFromMemory(NULL, s_gfx_debug);
    st_shader->shader = shader;
    st_shader->objects[GFX_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[GFX_SHADER_TILEMAPS_IDX] = GetShaderLocation(shader, SHADER_TILEMAPS_NAME);
    st_shader->objects[GFX_SHADER_TILESET_IDX]  = GetShaderLocation(shader, SHADER_TILESET_NAME);
    st_shader->objects[GFX_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);
    st_shader->objects[GFX_SHADER_DBGMODE_IDX]  = GetShaderLocation(shader, "debug_mode");

    (void) dev;
}


void zvb_render_text_mode(zvb_render_t* render, zvb_t* zvb)
{
    /* Update the palette to flush the changes to the shader */
    zvb_shader_t* st_shader = &render->shaders[SHADER_TEXT];
    const Shader shader = st_shader->shader;
    const int mode_idx         = st_shader->objects[TEXT_SHADER_VIDMODE_IDX];
    const int tilemaps_idx     = st_shader->objects[TEXT_SHADER_TILEMAPS_IDX];
    const int font_idx         = st_shader->objects[TEXT_SHADER_FONT_IDX];
    const int cursor_pos_idx   = st_shader->objects[TEXT_SHADER_CURPOS_IDX];
    const int cursor_color_idx = st_shader->objects[TEXT_SHADER_CURCOLOR_IDX];
    const int cursor_char_idx  = st_shader->objects[TEXT_SHADER_CURCHAR_IDX];
    const int scroll_idx       = st_shader->objects[TEXT_SHADER_TSCROLL_IDX];
    const int palette_idx      = st_shader->objects[TEXT_SHADER_PALETTE_IDX];

    /* Get the cursor position and its color */
    zvb_text_info_t info;
    zvb_text_update(&zvb->text, &info);

    BeginShaderMode(shader);
        /* Transfer all the texture to the GPU */
        SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
        SetShaderValueTexture(shader, palette_idx, zvb_pal_texture(&zvb->palette));
        SetShaderValueTexture(shader, tilemaps_idx, *zvb_tilemap_texture(&zvb->layers));
        SetShaderValueTexture(shader, font_idx, zvb_font_texture(&zvb->font));
        /* Transfer the text-related variables */
        SetShaderValue(shader, cursor_pos_idx,   &info.pos, SHADER_UNIFORM_IVEC2);
        SetShaderValue(shader, cursor_color_idx, &info.color, SHADER_UNIFORM_IVEC2);
        SetShaderValue(shader, cursor_char_idx,  &info.charidx, SHADER_UNIFORM_INT);
        SetShaderValue(shader, scroll_idx,  &info.scroll, SHADER_UNIFORM_IVEC2);

        /* Flip the screen in Y since OpenGL treats (0,0) as the bottom left pixel of the screen */
        DrawTextureRec(zvb->tex_dummy.texture,
                        (Rectangle){ 0, 0,
                                     ZVB_MAX_RES_WIDTH,
                                     zvb->flipped_y ? -ZVB_MAX_RES_HEIGHT : ZVB_MAX_RES_HEIGHT },
                        (Vector2){ 0, 0 },
                        WHITE);
    EndShaderMode();
}


/**
 * @brief Render the debug textures when we are in text mode.
 * The `zvb_render` function must be called first.
 */
void zvb_render_debug_text_mode(zvb_render_t* render, zvb_t* zvb)
{
    /* Since we want to generate a debug texture, we only need to set it to debug mode */
    zvb_shader_t* st_shader = &render->shaders[SHADER_TEXT_DEBUG];
    const Shader shader = st_shader->shader;
    const int mode_idx     = st_shader->objects[TEXT_SHADER_VIDMODE_IDX];
    const int dbg_mode_idx = st_shader->objects[TEXT_SHADER_DBGMODE_IDX];
    const int tilemaps_idx = st_shader->objects[TEXT_SHADER_TILEMAPS_IDX];
    const int font_idx     = st_shader->objects[TEXT_SHADER_FONT_IDX];
    const int palette_idx  = st_shader->objects[TEXT_SHADER_PALETTE_IDX];
    /* Include the debug grid in the final texture width. Add one pixel to show a red outline */
    const int grid_thickness = 1;
    const int width = TEXT_MAXIMUM_COLUMNS * (TEXT_CHAR_WIDTH + grid_thickness) + 1;
    const int height = TEXT_MAXIMUM_LINES * (TEXT_CHAR_HEIGHT + grid_thickness) + 1;

    /* Tilemap mode */
    int dbg_mode = TEXT_DEBUG_LAYER0_MODE;
    BeginTextureMode(zvb->debug_tex[DBG_TILEMAP_LAYER0]);
        BeginShaderMode(shader);
            ClearBackground(BLANK);
            /* Transfer all the texture to the GPU */
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
            SetShaderValueTexture(shader, palette_idx, zvb_pal_texture(&zvb->palette));
            SetShaderValueTexture(shader, tilemaps_idx, *zvb_tilemap_texture(&zvb->layers));
            SetShaderValueTexture(shader, font_idx, zvb_font_texture(&zvb->font));
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, width, height },
                            /* Since the texture is bigger than the content, render the content at the top left */
                            (Vector2){ 0, ZVB_DBG_RES_HEIGHT-height },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    /* Font mode, no need to reload all the textures, they are all in the shaders already */
    dbg_mode = TEXT_DEBUG_LAYER1_MODE;
    BeginTextureMode(zvb->debug_tex[DBG_TILEMAP_LAYER1]);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, width, height },
                            /* Since the texture is bigger than the content, render the content at the top left */
                            (Vector2){ 0, ZVB_DBG_RES_HEIGHT-height },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    dbg_mode = TEXT_DEBUG_FONT_MODE;
    RenderTexture* texture = &zvb->debug_tex[DBG_FONT];
    BeginTextureMode(*texture);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();
}


/**
 * @brief Render the screen in bitmap mode
 */

void zvb_render_bitmap_mode(zvb_render_t* render, zvb_t* zvb)
{
    zvb_shader_t* st_shader = &render->shaders[SHADER_BITMAP];
    const Shader shader = st_shader->shader;
    const int mode_idx     = st_shader->objects[GFX_SHADER_VIDMODE_IDX];
    const int tileset_idx  = st_shader->objects[GFX_SHADER_TILESET_IDX];
    const int palette_idx  = st_shader->objects[GFX_SHADER_PALETTE_IDX];

    BeginShaderMode(shader);
        SetShaderValueTexture(shader, palette_idx, zvb_pal_texture(&zvb->palette));
        /* Transfer all the texture to the GPU */
        SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
        SetShaderValueTexture(shader, tileset_idx, *zvb_tileset_texture(&zvb->tileset));

        /* Flip the screen in Y since OpenGL treats (0,0) as the bottom left pixel of the screen */
        DrawTextureRec(zvb->tex_dummy.texture,
                        (Rectangle){ 0, 0,
                                     ZVB_MAX_RES_WIDTH,
                                     zvb->flipped_y ? -ZVB_MAX_RES_HEIGHT : ZVB_MAX_RES_HEIGHT },
                        (Vector2){ 0, 0 },
                        WHITE);
    EndShaderMode();
}


/**
 * @brief Render the screen in graphics mode
 */
void zvb_render_gfx_mode(zvb_render_t* render, zvb_t* zvb)
{
    zvb_shader_t* st_shader = &render->shaders[SHADER_GFX];
    const Shader shader = st_shader->shader;

    const int mode_idx     = st_shader->objects[GFX_SHADER_VIDMODE_IDX];
    const int tilemaps_idx = st_shader->objects[GFX_SHADER_TILEMAPS_IDX];
    const int tileset_idx  = st_shader->objects[GFX_SHADER_TILESET_IDX];
    const int sprites_idx  = st_shader->objects[GFX_SHADER_SPRITES_IDX];
    const int scroll0_idx  = st_shader->objects[GFX_SHADER_SCROLL0_IDX];
    const int scroll1_idx  = st_shader->objects[GFX_SHADER_SCROLL1_IDX];
    const int palette_idx  = st_shader->objects[GFX_SHADER_PALETTE_IDX];

    BeginShaderMode(shader);
        /* Transfer all the texture to the GPU */
        SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
        SetShaderValueTexture(shader, palette_idx, zvb_pal_texture(&zvb->palette));
        SetShaderValueTexture(shader, tilemaps_idx, *zvb_tilemap_texture(&zvb->layers));
        SetShaderValueTexture(shader, tileset_idx, *zvb_tileset_texture(&zvb->tileset));
        SetShaderValueTexture(shader, sprites_idx, *zvb_sprites_texture(&zvb->sprites));
        /* Transfer the text-related variables */
        SetShaderValue(shader, scroll0_idx,  &zvb->ctrl.l0_scroll_x, SHADER_UNIFORM_IVEC2);
        SetShaderValue(shader, scroll1_idx,  &zvb->ctrl.l1_scroll_x, SHADER_UNIFORM_IVEC2);

        /* Flip the screen in Y since OpenGL treats (0,0) as the bottom left pixel of the screen */
        DrawTextureRec(zvb->tex_dummy.texture,
                        (Rectangle){ 0, 0,
                                     ZVB_MAX_RES_WIDTH,
                                     zvb->flipped_y ? -ZVB_MAX_RES_HEIGHT : ZVB_MAX_RES_HEIGHT },
                        (Vector2){ 0, 0 },
                        WHITE);
    EndShaderMode();
}


void zvb_render_debug_gfx_mode(zvb_render_t* render, zvb_t* zvb)
{
    /* Since we want to generate a debug texture, we only need to set it to debug mode */
    zvb_shader_t* st_shader = &render->shaders[SHADER_GFX_DEBUG];
    const Shader shader = st_shader->shader;
    const int mode_idx     = st_shader->objects[GFX_SHADER_VIDMODE_IDX];
    const int dbg_mode_idx = st_shader->objects[GFX_SHADER_DBGMODE_IDX];
    const int tilemaps_idx = st_shader->objects[GFX_SHADER_TILEMAPS_IDX];
    const int tileset_idx  = st_shader->objects[GFX_SHADER_TILESET_IDX];
    const int palette_idx  = st_shader->objects[GFX_SHADER_PALETTE_IDX];

    int dbg_mode = GFX_DEBUG_LAYER0_MODE;
    RenderTexture* texture = &zvb->debug_tex[DBG_TILEMAP_LAYER0];

    BeginTextureMode(*texture);
        BeginShaderMode(shader);
            /* Transfer all the texture to the GPU */
            SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            SetShaderValueTexture(shader, palette_idx, zvb_pal_texture(&zvb->palette));
            SetShaderValueTexture(shader, tilemaps_idx, *zvb_tilemap_texture(&zvb->layers));
            SetShaderValueTexture(shader, tileset_idx, *zvb_tileset_texture(&zvb->tileset));

            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    /* Debug layer 1, tell the shader to debug layer 1 in the mode (bit 30) */
    dbg_mode = GFX_DEBUG_LAYER1_MODE;
    texture = &zvb->debug_tex[DBG_TILEMAP_LAYER1];
    BeginTextureMode(*texture);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    /* Debug the tileset, in this case, we have 16x32 tiles at most */
    dbg_mode = GFX_DEBUG_TILESET_MODE;
    texture = &zvb->debug_tex[DBG_TILESET];
    BeginTextureMode(*texture);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    /* Palette mode */
    dbg_mode = GFX_DEBUG_PALETTE_MODE;
    texture = &zvb->debug_tex[DBG_PALETTE];
    BeginTextureMode(*texture);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();
}
