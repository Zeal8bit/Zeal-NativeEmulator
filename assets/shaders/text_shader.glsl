/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define PALETTE_SIZE    256.0
#define PALETTE_LAST    (PALETTE_SIZE - 1)

#define MODE_TEXT_320   1

#define CHAR_COUNT      256
#define CHAR_HEIGHT     12
#define CHAR_WIDTH      8
#define FONT_TEX_WIDTH  (256 * CHAR_WIDTH)
#define FONT_TEX_HEIGHT (CHAR_HEIGHT)

#define SCREEN_WIDTH    (640)
#define SCREEN_HEIGHT   (480)

#define MAX_X           (80)
#define MAX_Y           (40)

#define TILEMAP_ENTRIES  3199.0

#ifdef OPENGL_ES
precision highp float;
precision highp int;
#endif

in vec3 vertexPos;
in vec2 fragTexCoord;
in vec4 fragColor;
// layout(origin_upper_left, pixel_center_integer) in vec4 gl_FragCoord;

uniform sampler2D   texture0;
uniform sampler2D   tilemaps;
uniform sampler2D   font;
uniform sampler2D   palette;
uniform int         video_mode;

/* Cursor display related */
uniform ivec2       curpos;
uniform ivec2       curcolor; // .x is the background color, .y is the foreground
uniform int         curchar;
uniform ivec2       scroll;


out vec4 finalColor;

vec4 text_mode(ivec2 flipped) {
    ivec2 char_size = ivec2(CHAR_WIDTH, CHAR_HEIGHT);
    ivec2 char_pos = flipped / char_size;
    float tile_idx;
    float bg_color;
    float fg_color;

    if (char_pos == curpos) {
        tile_idx = float(curchar) / 255.01;
        bg_color = (float(curcolor.x) + 0.5) / PALETTE_SIZE;
        fg_color = (float(curcolor.y) + 0.5) / PALETTE_SIZE;
    } else {
        /* Take scrolling into account */
        char_pos.x = (char_pos.x + scroll.x) % MAX_X;
        char_pos.y = (char_pos.y + scroll.y) % MAX_Y;
        int char_idx = char_pos.x + char_pos.y * MAX_X;
        /* Added 0.1 to the divider to make sure we don't go beyond 1.0 */
        float float_idx = float(char_idx) / (TILEMAP_ENTRIES + 0.1);
        vec4 attr = texture(tilemaps, vec2(float_idx, 1.0));
         /* The tile number is in the RED attribute ([0.0,1.0]) */
        tile_idx = attr.r;
        /* We can use attr.a and attr.b out of the box since they are 8-bit big, just like the palette */
        fg_color = attr.a;
        bg_color = attr.b;
    }

    // Get the address of the pixel to show
    ivec2 in_tile = flipped % char_size;
    // As the address will be used to get a pixel from the texture, it must also be between 0.0 and 1.0
    float coord_x = tile_idx * float((CHAR_COUNT -  1) * CHAR_WIDTH);
    vec2 addr = (vec2(coord_x, 0.0) + vec2(in_tile))
               / vec2(FONT_TEX_WIDTH, CHAR_HEIGHT);

    // Check whether the color to show is foreground or background
    float is_fg = texture(font, addr).r;

    if (is_fg > 0.5f) {
        return texture(palette, vec2(fg_color, 0.5));
    } else {
        return texture(palette, vec2(bg_color, 0.5));
    }
}

void main() {
    // Create absolute coordinates, with (0,0) at the top-left
    ivec2 flipped = ivec2(gl_FragCoord.x, gl_FragCoord.y);
    // 320x240px mode, scale by 2
    if (video_mode == MODE_TEXT_320) {
        flipped.x = flipped.x / 2;
        flipped.y = flipped.y / 2;
    }

    finalColor = text_mode(flipped);
}
