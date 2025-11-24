/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"

/**
 * @brief Macros listing of all the objects in the shaders
 */
#define TEXT_SHADER_VIDMODE_IDX     0
#define TEXT_SHADER_TILEMAPS_IDX    1
#define TEXT_SHADER_FONT_IDX        2
#define TEXT_SHADER_PALETTE_IDX     3
#define TEXT_SHADER_CURPOS_IDX      4
#define TEXT_SHADER_CURCOLOR_IDX    5
#define TEXT_SHADER_CURCHAR_IDX     6
#define TEXT_SHADER_TSCROLL_IDX     7
#define TEXT_SHADER_DBGMODE_IDX     4

#define TEXT_SHADER_OBJ_COUNT       8


#define GFX_SHADER_VIDMODE_IDX      0
#define GFX_SHADER_TILEMAPS_IDX     1
#define GFX_SHADER_TILESET_IDX      2
#define GFX_SHADER_SPRITES_IDX      3
#define GFX_SHADER_SCROLL0_IDX      4
#define GFX_SHADER_SCROLL1_IDX      5
#define GFX_SHADER_PALETTE_IDX      6
#define GFX_SHADER_DBGMODE_IDX      3

#define GFX_SHADER_OBJ_COUNT        7

#define ZVB_SHADER_MAX_OBJ_COUNT    8


/* Special mode to tell the shader to debug the texture */
#define TEXT_DEBUG_MODE             0xffffffff
#define GFX_DEBUG_TILESET_MODE      0
#define GFX_DEBUG_LAYER0_MODE       1
#define GFX_DEBUG_LAYER1_MODE       2
#define GFX_DEBUG_PALETTE_MODE      3

#define TEXT_DEBUG_FONT_MODE        0
#define TEXT_DEBUG_LAYER0_MODE      1
#define TEXT_DEBUG_LAYER1_MODE      2
#define TEXT_DEBUG_PALETTE_MODE     3


typedef enum {
    SHADER_TEXT = 0,
    SHADER_GFX,
    SHADER_BITMAP,
    SHADER_GFX_DEBUG,
    SHADER_TEXT_DEBUG,
    SHADERS_COUNT,
} zvb_shaders_type_t;

typedef struct {
    Shader shader;
    int    objects[ZVB_SHADER_MAX_OBJ_COUNT];
} zvb_shader_t;


typedef struct zvb_render_t {
    zvb_shader_t    shaders[SHADERS_COUNT];
} zvb_render_t;
