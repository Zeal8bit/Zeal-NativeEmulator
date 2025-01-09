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
uniform sampler2D   tileset;
uniform vec3        palette[PALETTE_SIZE]; 
uniform int         video_mode; 

// uniform ivec2       scroll;


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

vec4 gfx_mode(vec2 flipped, bool mode_320, bool color_4bit) {
    // 320x240px mode, scale by 2
    if (mode_320) {
        flipped.x = flipped.x / 2;
        flipped.y = flipped.y / 2;
    }

    ivec2 tile_pos = ivec2(int(flipped.x) / TILE_WIDTH, int(flipped.y) / TILE_HEIGHT);
    // TODO: implement scrolling
    ivec2 scroll = ivec2(0, 0);
    /* Take scrolling into account */
    tile_pos.x = (tile_pos.x + scroll.x) % MAX_X;
    tile_pos.y = (tile_pos.t + scroll.y) % MAX_Y;
    int tile_idx = tile_pos.x + tile_pos.y * MAX_X;

    /* Get the address of the pixel to show within the 16x16 tile. Get it in one dimension */
    int in_tile = (int(flipped.x) % TILE_WIDTH) + ((int(flipped.y) % TILE_HEIGHT) * TILE_WIDTH);

    /* Added 0.1 to the divider to make sure we don't go beyond 1.0 */
    float float_idx = float(tile_idx) / (TILEMAP_ENTRIES + 0.1);
    vec4 attr = texture(tilemaps, vec2(float_idx, 1.0));

    /* Get the two indexes */
    int l0_idx = int(attr.r * (TILE_COUNT - 1));
    int l1_idx = int(attr.g * (TILE_COUNT - 1));

    float l0_color = color_from_idx(l0_idx, in_tile);
    float l1_color = color_from_idx(l1_idx, in_tile);

    int l0_icolor = int(l0_color * 255);
    int l1_icolor = int(l1_color * 255);

    /* If layer1 color index is 0, count it as transparent */
    if (l1_color == 0) {
        return vec4(palette[l0_icolor], 1.0f);
    } else {
        return vec4(palette[l1_icolor], 1.0f);
    }
}

void main() {
    // Create absolute coordinates, with (0,0) at the top-left 
    vec2 flipped   = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) 
                   * vec2(SCREEN_WIDTH, SCREEN_HEIGHT);

    finalColor = gfx_mode(flipped,
                          video_mode == MODE_GFX_320_8BIT || video_mode == MODE_GFX_320_4BIT,
                          video_mode == MODE_GFX_640_4BIT || video_mode == MODE_GFX_320_4BIT);
}
