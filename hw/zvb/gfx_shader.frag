#version 300 es

#define MODE_GFX_640_8BIT   4
#define MODE_GFX_320_8BIT   5
#define MODE_GFX_640_4BIT   6
#define MODE_GFX_320_4BIT   7


#define PALETTE_SIZE    256
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

#define TILEMAP_ENTRIES     3199
/* The tileset texture is stored with 4 pixels per color: 64KB / sizeof(Color) = 16KB */
#define TILESET_TEX_WIDTH   (16*1024)
/* Size of a single tile in bytes: 256 */
#define TILESET_SIZE        (256)

#define SIZEOF_COLOR        (4)

precision highp float;
precision highp int;

in vec3 vertexPos;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D   texture0;
uniform sampler2D   tilemaps;
uniform sampler2D   sprites;
uniform sampler2D   tileset;
uniform vec3        palette[PALETTE_SIZE];
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
    vec2 addr = vec2(float(final_idx / SIZEOF_COLOR) / (float(TILESET_TEX_WIDTH) + 0.0001), 0.0);
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


vec4 get_attr(vec2 flipped, ivec2 scroll) {
    ivec2 orig = ivec2(int(flipped.x), int(flipped.y));
    ivec2 pix_pos = orig + scroll;

    /* Take scrolling into account */
    pix_pos.x = (pix_pos.x % MAX_PIXEL_X);
    pix_pos.y = (pix_pos.y % MAX_PIXEL_Y);

    ivec2 tile_pos = ivec2(int(pix_pos.x) / TILE_WIDTH, int(pix_pos.y) / TILE_HEIGHT);
    int tile_idx = tile_pos.x + tile_pos.y * MAX_X;

    /* Get the address of the pixel to show within the 16x16 tile. Get it in one dimension */
    int in_tile = (int(pix_pos.x) % TILE_WIDTH) + ((int(pix_pos.y) % TILE_HEIGHT) * TILE_WIDTH);

    /* Added 0.1 to the divider to make sure we don't go beyond 1.0 */
    float float_idx = float(tile_idx) / (float(TILEMAP_ENTRIES) + 0.1);
    vec4 ret = texture(tilemaps, vec2(float_idx, 1.0));

    /* Replace unused B attribute with in_tile value */
    ret.b = float(in_tile);
    return ret;
}


vec4 gfx_mode(vec2 flipped, bool mode_320, bool color_4bit) {
    vec4 attr_l0 = get_attr(flipped, scroll_l0);
    vec4 attr_l1 = get_attr(flipped, scroll_l1);

    if (color_4bit) {
        /* In 4-bit mode, the original index needs to be divided by 2 (so that 255 is the last tile)
         * of the first half */
        int l0_idx = int(attr_l0.r * float(TILE_COUNT - 1));
        int tileset_offset = ((int(attr_l0.a * 15.0) & 1) != 0) ? 256 : 0;
        l0_idx += tileset_offset;
        /* Get the color out of that tile's pixel, between 0 and 15 */
        int color = color_from_idx(l0_idx, int(attr_l0.b), color_4bit);
        /* If the layer1's lowest bit was 1, we need to get the right pixel (lowest nibble) */
        /* The color is between 0 and 15, append the palette index to it */
        int palette_idx = int(attr_l0.g * 255.0) & 0xf0;
        /* No transparency in 4-bit mode */
        return vec4(palette[color + palette_idx], 1.0f);
    } else {
        /* 8-bit color mode */
        /* Get the two indexes */
        int l0_idx = int(attr_l0.r * float(TILE_COUNT - 1));
        int l1_idx = int(attr_l1.g * float(TILE_COUNT - 1));

        int l0_icolor = color_from_idx(l0_idx, int(attr_l0.b), color_4bit);
        int l1_icolor = color_from_idx(l1_idx, int(attr_l1.b), color_4bit);

        /* If layer1 color index is 0, count it as transparent */
        if (l1_icolor == 0) {
            return vec4(palette[l0_icolor], 0.0f);
        } else {
            return vec4(palette[l1_icolor], 1.0f);
        }
    }
}


void main() {
    // Create absolute coordinates, with (0,0) at the top-left
    vec2 flipped   = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y)
                   * vec2(SCREEN_WIDTH, SCREEN_HEIGHT);
    bool mode_320   = video_mode == MODE_GFX_320_8BIT || video_mode == MODE_GFX_320_4BIT;
    bool color_4bit = video_mode == MODE_GFX_640_4BIT || video_mode == MODE_GFX_320_4BIT;

    if (mode_320) {
        flipped.x = flipped.x / 2.0;
        flipped.y = flipped.y / 2.0;
    }

    vec4 layers_color = gfx_mode(flipped, mode_320, color_4bit);

    vec4 sprite_color = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < 256; i += 2) {

        vec4 fst_attr = texture(sprites, vec2(float(i + 0) / 255.0, 1.0));
        vec4 snd_attr = texture(sprites, vec2(float(i + 1) / 255.0, 1.0));

        vec2 sprite_pos   = fst_attr.xy - vec2(TILE_WIDTH, TILE_HEIGHT);
        int tile_number   = int(fst_attr.z);
        int palette_msk   = int(fst_attr.w);
        float f_behind_fg = snd_attr.x;
        float f_flip_y    = snd_attr.y;
        float f_flip_x    = snd_attr.z;

        /* Ignore the palette in 8-bit mode */
        if (!color_4bit) {
            palette_msk = 0;
        }

        /* Check if the sprite is in bounds! */
        if (flipped.x >= sprite_pos.x &&
            flipped.x <  sprite_pos.x + float(TILE_WIDTH) &&
            flipped.y >= sprite_pos.y &&
            flipped.y <  sprite_pos.y + float(TILE_HEIGHT) &&
            /* Check if we have to show the layer1 instead:
             * If the layers_color variable comes from layer1, the `w` field is not 0 */
            (f_behind_fg < 0.1 || layers_color.w < 0.5)
            )
        {
            vec2  pix_pos = flipped - sprite_pos;
            if (f_flip_y > 0.5) {
                pix_pos.y = float(TILE_HEIGHT - 1) - pix_pos.y;
            }
            if (f_flip_x > 0.5) {
                pix_pos.x = float(TILE_WIDTH - 1) - pix_pos.x;
            }
            /* Get the address of the pixel to show within the 16x16 tile. Get it in one dimension */
            int in_tile = int(pix_pos.x) + int(pix_pos.y) * TILE_WIDTH;
            int icolor = color_from_idx(tile_number, in_tile, color_4bit);
            if (icolor != 0) {
                sprite_color = vec4(palette[palette_msk + icolor], 1.0);
            }
        }
    }

    if (sprite_color.a > 0.0) {
        finalColor = sprite_color;
    } else {
        finalColor = vec4(layers_color.xyz, 1.0);
    }
}
