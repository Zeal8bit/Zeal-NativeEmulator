/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define MODE_BITMAP_256   2
#define MODE_BITMAP_320   3

#define PALETTE_SIZE    256.0
#define PALETTE_LAST    (PALETTE_SIZE - 1)

#define BITMAP_256_BORDER   32
#define BITMAP_320_BORDER   20

#define TILESET_TEX_WIDTH   256
#define TILESET_TEX_HEIGHT  256

#ifdef OPENGL_ES
precision highp float;
precision highp int;
#endif

in vec3 vertexPos;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D   texture0;
uniform sampler2D   tileset;
uniform sampler2D   palette;
uniform int         video_mode;


out vec4 finalColor;

/* Returns a color index that can be retrieved from the palette */
int color_from_idx(int idx)
{
    ivec2 coordinates = ivec2(idx % TILESET_TEX_WIDTH, idx / TILESET_TEX_WIDTH);
    float byte_value = texelFetch(tileset, coordinates, 0).r;
    return int(byte_value * 255.0 + 0.5);
}


void main() {
    /* Divide by 2 since we are in 320x240 mode */
    ivec2 coord = ivec2(gl_FragCoord.x, gl_FragCoord.y) / 2;
    int index = 0;

    if (video_mode == MODE_BITMAP_256) {
        /* In 256 mode, there are a left and right border of 32 pixels each */
        index = coord.y * 256 + (coord.x - 32);
    } else {
        /* In 320 mode, there are a top and bottom borders of 20 pixels each */
        index = (coord.y - 20) * 320 + coord.x;
    }

    if (
        (video_mode == MODE_BITMAP_256 && (coord.x < BITMAP_256_BORDER || coord.x >= BITMAP_256_BORDER + 256)) ||
        (video_mode == MODE_BITMAP_320 && (coord.y < BITMAP_320_BORDER || coord.y >= BITMAP_320_BORDER + 200))
       )
    {
        /* Last value is used as the border color */
        index = 0xFFFF;
    }

    /* Convert the color index into an RGB color */
    int color_idx = color_from_idx(index);
    finalColor = texelFetch(palette, ivec2(color_idx, 0), 0);
}
