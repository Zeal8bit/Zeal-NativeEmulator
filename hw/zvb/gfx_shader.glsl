#version 330 core

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
 */
float color_from_idx(int idx, int offset)
{
    int final_idx = (idx * TILESET_SIZE + offset);
    int rem_idx = final_idx % SIZEOF_COLOR;
    vec2 addr = vec2((final_idx / SIZEOF_COLOR) / (TILESET_TEX_WIDTH + 0.0001), 0.0);
    /* Get one set of color per layer, each containing 4 pixels */
    vec4 set = texture(tileset, addr);
    if (rem_idx == 0) {
        return set.r;
    } else if (rem_idx == 1) {
        return set.g;
    } else if (rem_idx == 2) {
        return set.b;
    } else if (rem_idx == 3) {
        return set.a;
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
    float float_idx = float(tile_idx) / (TILEMAP_ENTRIES + 0.1);
    vec4 ret = texture(tilemaps, vec2(float_idx, 1.0));

    /* Replace unused B aatribute with in_tile value */
    ret.b = float(in_tile);
    return ret;
}


vec4 gfx_mode(vec2 flipped, bool mode_320, bool color_4bit) {
    vec4 attr_l0 = get_attr(flipped, scroll_l0);
    vec4 attr_l1 = get_attr(flipped, scroll_l1);

    /* Get the two indexes */
    int l0_idx = int(attr_l0.r * (TILE_COUNT - 1));
    int l1_idx = int(attr_l1.g * (TILE_COUNT - 1));

    float l0_color = color_from_idx(l0_idx, int(attr_l0.b));
    float l1_color = color_from_idx(l1_idx, int(attr_l1.b));

    int l0_icolor = int(l0_color * 255);
    int l1_icolor = int(l1_color * 255);

    /* If layer1 color index is 0, count it as transparent */
    if (l1_color == 0) {
        return vec4(palette[l0_icolor], 0.0f);
    } else {
        return vec4(palette[l1_icolor], 1.0f);
    }
}


void main() {
    // Create absolute coordinates, with (0,0) at the top-left
    vec2 flipped   = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y)
                   * vec2(SCREEN_WIDTH, SCREEN_HEIGHT);
    bool mode_320   = video_mode == MODE_GFX_320_8BIT || video_mode == MODE_GFX_320_4BIT;
    bool color_4bit = video_mode == MODE_GFX_320_8BIT || video_mode == MODE_GFX_320_4BIT;

    if (mode_320) {
        flipped.x = flipped.x / 2;
        flipped.y = flipped.y / 2;
    }

    vec4 layers_color = gfx_mode(flipped, mode_320, color_4bit);

    vec4 sprite_color = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < 256; i += 2) {

        vec4 fst_attr = texture(sprites, vec2((i + 0) / 255.0, 1.0));
        vec4 snd_attr = texture(sprites, vec2((i + 1) / 255.0, 1.0));

        vec2 sprite_pos   = fst_attr.xy - vec2(TILE_WIDTH, TILE_HEIGHT);
        int tile_number   = int(fst_attr.z);
        // float palette     = fst_attr.w;
        float f_behind_fg = snd_attr.x;
        float f_flip_y    = snd_attr.y;
        float f_flip_x    = snd_attr.z;

        /* Check if the sprite is in bounds! */
        if (flipped.x >= sprite_pos.x &&
            flipped.x <  sprite_pos.x + TILE_WIDTH &&
            flipped.y >= sprite_pos.y &&
            flipped.y <  sprite_pos.y + TILE_HEIGHT &&
            /* Check if we have to show the layer1 instead:
             * If the layers_color variable comes from layer1, the `w` field is not 0 */
            (f_behind_fg < 0.1 || layers_color.w < 0.5)
            )
        {
            vec2  pix_pos = flipped - sprite_pos;
            if (f_flip_y > 0.5) {
                pix_pos.y = TILE_HEIGHT - 1 - pix_pos.y;
            }
            if (f_flip_x > 0.5) {
                pix_pos.x = TILE_WIDTH - 1 - pix_pos.x;
            }
            /* Get the address of the pixel to show within the 16x16 tile. Get it in one dimension */
            int   in_tile = int(pix_pos.x) + int(pix_pos.y) * TILE_WIDTH;
            float color = color_from_idx(tile_number, in_tile);
            int   icolor = int(color * 255);
            if (icolor != 0) {
                sprite_color = vec4(palette[icolor], 1.0);
            }
        }
    }

    if (sprite_color.a > 0.0) {
        finalColor = sprite_color;
    } else {
        finalColor = vec4(layers_color.xyz, 1.0);
    }
}
