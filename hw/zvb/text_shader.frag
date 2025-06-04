#version 300 es

#define PALETTE_SIZE    256
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

precision highp float;
precision highp int;

in vec3 vertexPos;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D   texture0;
uniform sampler2D   tilemaps;
uniform sampler2D   font;
uniform vec3        palette[PALETTE_SIZE];
uniform int         video_mode;

/* Cursor display related */
uniform ivec2       curpos;
uniform ivec2       curcolor; // .x is the background color, .y is the foreground
uniform int         curchar;
uniform ivec2       scroll;


out vec4 finalColor;

vec4 text_mode(vec2 flipped, bool mode_320) {
    // 320x240px mode, scale by 2
    if (mode_320) {
        flipped.x = flipped.x / 2.0;
        flipped.y = flipped.y / 2.0;
    }

    ivec2 char_pos = ivec2(int(flipped.x) / CHAR_WIDTH, int(flipped.y) / CHAR_HEIGHT);
    float tile_idx;
    int bg_color;
    int fg_color;

    if (char_pos == curpos) {
        tile_idx = float(curchar) / 255.01;
        bg_color = curcolor.x;
        fg_color = curcolor.y;
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
        fg_color = int(floor(attr.a * 255.0 + 0.5));
        bg_color = int(floor(attr.b * 255.0 + 0.5));
    }

    // Get the address of the pixel to show
    ivec2 in_tile = ivec2(int(flipped.x) % CHAR_WIDTH, int(flipped.y) % CHAR_HEIGHT);


    // As the address will be used to get a pixel from the texture, it must also be between 0.0 and 1.0
    int tile_index = int(floor(tile_idx * 255.0 + 0.5));

    vec2 addr = (vec2(float(tile_index * CHAR_WIDTH + in_tile.x) + 0.5, float(in_tile.y) + 0.5))
            /  vec2(float(FONT_TEX_WIDTH), float(FONT_TEX_HEIGHT));

    // Check whether the color to show is foreground or background
    float is_fg = texture(font, addr).r;

    if (is_fg > 0.5f) {
        // Alpha channel (.w) for foreground color. In the texture,
        // the colors are between 0f and 1f, so to get back the original data,
        // multiply by 255 (0 becomes 0 and 1 becomes 255).
        return vec4(palette[fg_color], 1.0f);
    } else {
        // Blue channel (.z) for background color
        return vec4(palette[bg_color], 1.0f);
    }
}

void main() {
    // Create absolute coordinates, with (0,0) at the top-left
    vec2 flipped   = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y)
                   * vec2(SCREEN_WIDTH, SCREEN_HEIGHT);

    finalColor = text_mode(flipped, video_mode == MODE_TEXT_320);
}
