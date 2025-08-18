/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define MODE_GFX_640_8BIT   4
#define MODE_GFX_320_8BIT   5
#define MODE_GFX_640_4BIT   6
#define MODE_GFX_320_4BIT   7

#define PALETTE_SIZE    256.0
#define PALETTE_LAST    (PALETTE_SIZE - 1)

#define MODE_TEXT_320   1

#define TILE_COUNT      256
#define TILE_HEIGHT     16
#define TILE_WIDTH      16

#define SCREEN_WIDTH    (640)
#define SCREEN_HEIGHT   (480)

#define MAX_PIXEL_X     (1280)
#define MAX_PIXEL_Y     (640)
#define MAX_X           (80)
#define MAX_Y           (40)

#define TILEMAP_ENTRIES     3199.0
/* The tileset texture is stored with 4 pixels per color: 64KB / sizeof(Color) = 16KB */
#define TILESET_TEX_WIDTH   64
#define TILESET_TEX_HEIGHT  256
/* Size of a single tile in bytes: 256 */
#define TILESET_SIZE        (256)

#define SIZEOF_COLOR        (4)

#ifdef OPENGL_ES
precision highp float;
precision highp int;
#endif

in vec3 vertexPos;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D   texture0;
uniform sampler2D   tilemaps;
uniform sampler2D   sprites;
uniform sampler2D   tileset;
uniform sampler2D   palette;
uniform int         video_mode;
uniform ivec2       scroll_l0;
uniform ivec2       scroll_l1;


out vec4 finalColor;

/**
 * @param idx Index of the tile to render (from a tilemap layer)
 * @param offset Offset within the tile to get the pixel
 *
 * @returns color as an index in the palette (integer)
 */
int color_from_idx(int idx, int offset, bool color_4bit)
{
    bool lsb = true;
    int final_idx = (idx * TILESET_SIZE + offset);

    if (color_4bit) {
        lsb = (final_idx % 2) != 0;
        /* In 4-bit mode, we can simply divide the index by two to get the correct pixel(s) */
        final_idx = final_idx / 2;
    }
    /* Each pixel in the texture has 4 colors */
    int pixel_index = final_idx / SIZEOF_COLOR;
    ivec2 coordinates = ivec2(pixel_index % TILESET_TEX_WIDTH, pixel_index / TILESET_TEX_WIDTH);
    vec2 addr = (vec2(coordinates) + vec2(0.5, 0.5)) / vec2(64.0, 256.0);

    /* Get one set of color per layer, each containing 4 pixels */
    vec4 set = texture(tileset, addr);
    int channel = final_idx % SIZEOF_COLOR;
    float byte_value = (channel == 0) ? set.r :
                        (channel == 1) ? set.g :
                        (channel == 2) ? set.b : set.a;
    if (color_4bit) {
        /* Convert to 8-bit (0-255 range) */
        int byte_int = int(byte_value * 255.0);
        /* Extract 4-bit value */
        int color_4bit = lsb ? (byte_int & 0x0F) : (byte_int >> 4);
        /* Normalize 4-bit color to 0-1 range */
        return color_4bit;
    } else {
        return int(byte_value * 255.0);
    }
}


vec4 get_attr(ivec2 orig, ivec2 scroll, out ivec2 offset_in_tile) {
    ivec2 pix_pos = orig + scroll;

    /* Take scrolling into account */
    pix_pos.x = (pix_pos.x % MAX_PIXEL_X);
    pix_pos.y = (pix_pos.y % MAX_PIXEL_Y);

    ivec2 tile_pos = ivec2(int(pix_pos.x) / TILE_WIDTH, int(pix_pos.y) / TILE_HEIGHT);
    int tile_idx = tile_pos.x + tile_pos.y * MAX_X;

    /* Get the address of the pixel to show within the 16x16 tile. Get it in one dimension */
    offset_in_tile.x = int(pix_pos.x) % TILE_WIDTH;
    offset_in_tile.y = int(pix_pos.y) % TILE_HEIGHT;
    int in_tile = offset_in_tile.x + (offset_in_tile.y * TILE_WIDTH);

    /* Added 0.1 to the divider to make sure we don't go beyond 1.0 */
    float float_idx = float(tile_idx) / (TILEMAP_ENTRIES + 0.1);
    vec4 ret = texture(tilemaps, vec2(float_idx, 1.0));

    /* Replace unused B attribute with in_tile value */
    ret.b = float(in_tile);
    return ret;
}


vec4 gfx_mode(ivec2 flipped, bool mode_320, bool color_4bit) {
    ivec2 l0_offset;
    ivec2 l1_offset;
    vec4 attr_l0 = get_attr(flipped, scroll_l0, l0_offset);
    vec4 attr_l1 = get_attr(flipped, scroll_l1, l1_offset);

    if (color_4bit) {
        /* In 4-bit mode, the original index needs to be divided by 2 (so that 255 is the last tile)
         * of the first half */
        int l0_idx = int(attr_l0.r * float(TILE_COUNT - 1));
        /* Get the lowest nibble of layer1 (attributes), if 1, offset the tile to the next tileset */
        int attr = int(attr_l0.g * 255.0);
        l0_idx += (attr & 1) != 0 ? 256 : 0;
        /* Check for Flip Y attribute */
        if ((attr & 4) != 0) {
            l0_offset.y = (TILE_HEIGHT - 1) - l0_offset.y;
        }
        /* Check for Flip X attribute */
        if ((attr & 8) != 0) {
            l0_offset.x = (TILE_WIDTH - 1) - l0_offset.x;
        }
        int offset_in_tile = l0_offset.y * TILE_WIDTH + l0_offset.x;
        /* Get the color out of that tile's pixel, between 0 and 15 */
        int color = color_from_idx(l0_idx, offset_in_tile, color_4bit);
        /* If the layer1's lowest bit was 1, we need to get the right pixel (lowest nibble) */
        /* The color is between 0 and 15, append the palette index to it */
        int palette_idx = attr & 0xf0;
        /* No transparency in 4-bit mode */
        float color_idx = float(color + palette_idx) + 0.5;
        return texture(palette, vec2(color_idx / PALETTE_SIZE, 0.5));
    } else {
        /* 8-bit color mode */
        /* Get the two indexes */
        int l0_idx = int(attr_l0.r * float(TILE_COUNT - 1));
        int l1_idx = int(attr_l1.g * float(TILE_COUNT - 1));

        int l0_icolor = color_from_idx(l0_idx, int(attr_l0.b), color_4bit);
        int l1_icolor = color_from_idx(l1_idx, int(attr_l1.b), color_4bit);

        /* If layer1 color index is 0, count it as transparent */
        if (l1_icolor == 0) {
            vec4 color = texture(palette, vec2((float(l0_icolor) + 0.5) / PALETTE_SIZE, 0.5));
            color.a = 0.0;
            return color;
        } else {
            vec4 color = texture(palette, vec2((float(l1_icolor) + 0.5) / PALETTE_SIZE, 0.5));
            /* Alpha channel is already 1.0f */
            return color;
        }
    }
}


void main() {
    // Create absolute coordinates, with (0,0) at the top-left
    ivec2 flipped = ivec2(gl_FragCoord.x, gl_FragCoord.y);
    bool mode_320   = video_mode == MODE_GFX_320_8BIT || video_mode == MODE_GFX_320_4BIT;
    bool color_4bit = video_mode == MODE_GFX_640_4BIT || video_mode == MODE_GFX_320_4BIT;

    if (mode_320) {
        flipped.x = flipped.x / 2;
        flipped.y = flipped.y / 2;
    }
    vec2 fcoord = vec2(flipped);

    vec4 layers_color = gfx_mode(flipped, mode_320, color_4bit);

    vec4 sprite_color = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < 256; i += 2) {

        vec4 fst_attr = texture(sprites, vec2(float(i) / 255.5, 0.5));
        vec4 snd_attr = texture(sprites, vec2(float(i + 1) / 255.5, 0.5));

        vec2 sprite_pos   = fst_attr.xy - vec2(TILE_WIDTH, TILE_HEIGHT);
        int tile_number   = int(fst_attr.z);
        int palette_msk   = int(fst_attr.w);
        float f_behind_fg = snd_attr.x;
        float f_flip_y    = snd_attr.y;
        float f_flip_x    = snd_attr.z;
        float f_height_32 = snd_attr.w;

        float sprite_height = (f_height_32 > 0.5) ? 32.0 : 16.0;
        /* Ignore the palette in 8-bit mode */
        if (!color_4bit) {
            palette_msk = 0;
        }

        /* Check if the sprite is in bounds! */
        if (fcoord.x >= sprite_pos.x &&
            fcoord.x <  sprite_pos.x + float(TILE_WIDTH) &&
            fcoord.y >= sprite_pos.y &&
            fcoord.y <  sprite_pos.y + sprite_height &&
            /* Check if we have to show the layer1 instead:
             * If the layers_color variable comes from layer1, the `a` field is not 0 */
            (f_behind_fg < 0.1 || layers_color.a < 0.5)
            )
        {
            vec2 pix_pos = fcoord - sprite_pos;
            if (f_flip_y > 0.5) {
                pix_pos.y = sprite_height - 1.0 - pix_pos.y;
            }
            if (f_flip_x > 0.5) {
                pix_pos.x = float(TILE_WIDTH) - 1.0 - pix_pos.x;
            }
            /* Get the address of the pixel to show within the 16x16 tile. Get it in one dimension */
            int in_tile = int(pix_pos.x) + int(pix_pos.y) * TILE_WIDTH;
            int icolor = color_from_idx(tile_number, in_tile, color_4bit);
            if (icolor != 0) {
                float color_idx = float(palette_msk + icolor) + 0.5;
                sprite_color = texture(palette, vec2(color_idx / PALETTE_SIZE, 0.5));
            }
        }
    }

    if (sprite_color.a > 0.0) {
        finalColor = sprite_color;
    } else {
        finalColor = vec4(layers_color.xyz, 1.0);
    }
}
