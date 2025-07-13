#version 330 core

#define PALETTE_SIZE    256
#define PALETTE_LAST    (PALETTE_SIZE - 1)

#define MODE_TEXT_320   1

#define TEXT_DEBUG_TILEMAP  0
#define TEXT_DEBUG_FONT     1
#define TEXT_DEBUG_PALETTE  2

#define CHAR_COUNT      256
#define CHAR_HEIGHT     12
#define CHAR_WIDTH      8
#define GRID_THICKNESS  1
#define GRID_WIDTH      (CHAR_WIDTH + GRID_THICKNESS)
#define GRID_HEIGHT     (CHAR_HEIGHT + GRID_THICKNESS)
#define FONT_TEX_WIDTH  (256 * CHAR_WIDTH)
#define FONT_TEX_HEIGHT (CHAR_HEIGHT)

#define SCREEN_WIDTH    (640)
#define SCREEN_HEIGHT   (480)

#define MAX_X           (80)
#define MAX_Y           (40)

#define TILEMAP_ENTRIES  3199


in vec3 vertexPos;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D   texture0;
uniform sampler2D   tilemaps;
uniform sampler2D   font;
uniform vec3        palette[PALETTE_SIZE];
uniform int         video_mode;
uniform int         debug_mode;


out vec4 finalColor;

vec4 text_mode(ivec2 char_pos, ivec2 in_tile) {
    vec4 fg_color;
    vec4 bg_color;
    vec2 addr;

    if (debug_mode == TEXT_DEBUG_TILEMAP) {
        int char_idx = char_pos.x + char_pos.y * MAX_X;
        /* Added 0.1 to the divider to make sure we don't go beyond 1.0 */
        float float_idx = float(char_idx) / (TILEMAP_ENTRIES + 0.1);
        vec4 attr = texture(tilemaps, vec2(float_idx, 1.0));
        /* The tile number is in the RED attribute ([0.0,1.0]) */
        float tile_idx = attr.r;
        fg_color = vec4(palette[int(attr.a * 255)], 1.0f);
        bg_color = vec4(palette[int(attr.b * 255)], 1.0f);
        /* As the address will be used to get a pixel from the texture,
         * it must also be between 0.0 and 1.0 */
        addr = (vec2(tile_idx * (CHAR_COUNT -  1) * CHAR_WIDTH, 0.0) + in_tile)
            / vec2(FONT_TEX_WIDTH, CHAR_HEIGHT);
    } else {
        fg_color = vec4 (1.0, 1.0, 1.0, 1.0);
        bg_color = vec4 (0.0, 0.0, 0.0, 1.0);
        /* We only hav 16 characters per line for the font texture */
        int char_idx = char_pos.x + char_pos.y * 16;
        addr = (vec2(float(char_idx) * CHAR_WIDTH, 0.0) + in_tile)
            / vec2(FONT_TEX_WIDTH, CHAR_HEIGHT);
    }

    /* Check whether the color to show is foreground or background */
    float is_fg = texture(font, addr).r;
    if (is_fg > 0.5f) return fg_color;
    else return bg_color;
}

void main() {
    /* Cooridnates are different in debug mode since we have the grid */
    ivec2 ipos = ivec2(gl_FragCoord.x, gl_FragCoord.y);
    ivec2 in_cell = ivec2(ipos) % ivec2(GRID_WIDTH, GRID_HEIGHT);
    ivec2 grid_size = ivec2(GRID_WIDTH, GRID_HEIGHT);
    /* Get the cell the current is in */
    ivec2 cell_idx = ipos / grid_size;

    if (in_cell.x == 0 || in_cell.y == 0) {
        finalColor = vec4(0.65, 0.0, 0.0, 1.0);
    } else if (debug_mode == TEXT_DEBUG_PALETTE) {
        /* In this mode we have 16 colors per line */
        int color = cell_idx.y * 16 + cell_idx.x;
        finalColor = vec4(palette[color], 1.0f);
    } else {
        /* Convert it to a screen position */
        ivec2 cell_pos = cell_idx * ivec2(CHAR_WIDTH, CHAR_HEIGHT);
        /* Get the coordinate of the pixel in cell, without the grid */
        in_cell -= ivec2(GRID_THICKNESS, GRID_THICKNESS);
        ivec2 text_coords = cell_pos + in_cell;
        finalColor = text_mode(cell_idx, in_cell);
    }
}
