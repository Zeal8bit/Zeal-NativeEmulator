/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define MODE_GFX_320_8BIT   5
#define MODE_GFX_640_4BIT   6
#define MODE_GFX_320_4BIT   7

#define PALETTE_SIZE    256.0

#define TILE_COUNT      256
#define TILE_HEIGHT     16
#define TILE_WIDTH      16

#define MAX_PIXEL_X     (1280)
#define MAX_PIXEL_Y     (640)
#define MAX_X           (80)
#define TILEMAP_ENTRIES 3200.0

#define TILESET_TEX_WIDTH   64
#define TILESET_TEX_HEIGHT  256
#define TILESET_SIZE        256
#define SIZEOF_COLOR        4

#ifdef OPENGL_ES
precision highp float;
precision highp int;
#endif

in vec3 vertexPos;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D tilemaps;
uniform sampler2D tileset;
uniform sampler2D palette;
uniform int video_mode;
uniform int tile_number;
uniform int palette_mask;
uniform int flags;
uniform ivec2 sprite_pos;
uniform ivec2 sprite_size;
uniform ivec2 scroll_l1;

out vec4 finalColor;

#define FLAG_BEHIND_FG 1
#define FLAG_FLIP_Y    2
#define FLAG_FLIP_X    4
#define FLAG_HEIGHT_32 8

int color_from_idx(int idx, int offset, bool color_4bit)
{
    bool lsb = true;
    idx = color_4bit ? (idx & 0x1FF) : (idx & 0xFF);
    int final_idx = idx * TILESET_SIZE + offset;

    if (color_4bit) {
        lsb = (final_idx % 2) != 0;
        final_idx = final_idx / 2;
    }

    int pixel_index = final_idx / SIZEOF_COLOR;
    ivec2 coordinates = ivec2(pixel_index % TILESET_TEX_WIDTH, pixel_index / TILESET_TEX_WIDTH);
    vec4 set = texelFetch(tileset, coordinates, 0);
    int channel = final_idx % SIZEOF_COLOR;
    float byte_value = (channel == 0) ? set.r :
                       (channel == 1) ? set.g :
                       (channel == 2) ? set.b : set.a;

    if (color_4bit) {
        int byte_int = int(byte_value * 255.0 + 0.5);
        return lsb ? (byte_int & 0x0F) : (byte_int >> 4);
    }

    return int(byte_value * 255.0 + 0.5);
}

int foreground_color(ivec2 screen_pos)
{
    ivec2 pix_pos = screen_pos + scroll_l1;
    pix_pos.x = pix_pos.x % MAX_PIXEL_X;
    pix_pos.y = pix_pos.y % MAX_PIXEL_Y;

    ivec2 tile_pos = ivec2(pix_pos.x / TILE_WIDTH, pix_pos.y / TILE_HEIGHT);
    int tile_idx = tile_pos.x + tile_pos.y * MAX_X;
    int in_tile = (pix_pos.x % TILE_WIDTH) + (pix_pos.y % TILE_HEIGHT) * TILE_WIDTH;
    vec4 attr = texelFetch(tilemaps, ivec2(tile_idx, 0), 0);
    int l1_idx = int(attr.g * 255.0 + 0.5);

    return color_from_idx(l1_idx, in_tile, false);
}

void main()
{
    bool mode_320 = video_mode == MODE_GFX_320_8BIT || video_mode == MODE_GFX_320_4BIT;
    bool color_4bit = video_mode == MODE_GFX_640_4BIT || video_mode == MODE_GFX_320_4BIT;

    ivec2 local = ivec2(fragTexCoord * vec2(sprite_size));
    local.y = sprite_size.y - 1 - local.y;
    ivec2 screen_pos = sprite_pos + local;
    if (mode_320) {
        screen_pos.x = screen_pos.x / 2;
        screen_pos.y = screen_pos.y / 2;
    }

    if (!color_4bit && (flags & FLAG_BEHIND_FG) != 0 && foreground_color(screen_pos) != 0) {
        discard;
    }

    if ((flags & FLAG_FLIP_Y) != 0) {
        local.y = sprite_size.y - 1 - local.y;
    }
    if ((flags & FLAG_FLIP_X) != 0) {
        local.x = TILE_WIDTH - 1 - local.x;
    }

    int in_tile = local.x + local.y * TILE_WIDTH;
    int color = color_from_idx(tile_number, in_tile, color_4bit);

    if (color == 0) {
        discard;
    }

    int sprite_palette = color_4bit ? palette_mask : 0;
    finalColor = texelFetch(palette, ivec2(sprite_palette + color, 0), 0);
    finalColor.a = 1.0;
}
