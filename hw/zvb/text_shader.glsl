#version 330 core

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

#define TILEMAP_ENTRIES  3199


in vec3 vertexPos;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D   texture0;
uniform sampler2D   tilemaps;
uniform sampler2D   font;
uniform vec3        palette[PALETTE_SIZE]; 
uniform int         video_mode; 

out vec4 finalColor;

vec4 text_mode(vec2 flipped, bool mode_320) {
    // 320x240px mode, scale by 2
    if (mode_320) {
        flipped.x = flipped.x / 2;
        flipped.y = flipped.y / 2;
    }

    ivec2 char_pos = ivec2(int(flipped.x) / CHAR_WIDTH, int(flipped.y) / CHAR_HEIGHT);
    int   char_idx = char_pos.x + char_pos.y * 80;
    /* Added 0.1 to the divider to make sure we don't go beyond 1.0 */
    float float_idx = float(char_idx) / (TILEMAP_ENTRIES + 0.1);
    vec4 attr = texture(tilemaps, vec2(float_idx, 1.0));

    // Get the address of the pixel to show, the tile number is in the RED attribute ([0.0,1.0])
    ivec2 in_tile = ivec2(int(flipped.x) % CHAR_WIDTH, int(flipped.y) % CHAR_HEIGHT);
    // As the address will be used to get a pixel from the texture, it must also be between 0.0 and 1.0
    vec2 addr = (vec2(attr.r * (CHAR_COUNT -  1) * CHAR_WIDTH, 0.0) + in_tile)
               / vec2(FONT_TEX_WIDTH, CHAR_HEIGHT);

    // Check whether the color to show is foreground or background
    float is_fg = texture(font, addr).r;

    if (is_fg > 0.5f) {
        // Alpha channel (.w) for foreground color. In the texture,
        // the colors are between 0f and 1f, so to get back the original data,
        // multiply by 255 (0 becomes 0 and 1 becomes 255).
        return vec4(palette[int(attr.a * 255)], 1.0f);
    } else {
        // Blue channel (.z) for background color
        return vec4(palette[int(attr.b * 255)], 1.0f);
    }
}

void main() {
    // Create absolute coordinates, with (0,0) at the top-left 
    vec2 flipped   = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) 
                   * vec2(SCREEN_WIDTH, SCREEN_HEIGHT);

    finalColor = text_mode(flipped, video_mode == MODE_TEXT_320);
}
