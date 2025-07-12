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
#define GRID_THICKNESS  1
#define GRID_WIDTH      (TILE_WIDTH + GRID_THICKNESS)
#define GRID_HEIGHT     (TILE_HEIGHT + GRID_THICKNESS)

#define GFX_DEBUG_TILESET_MODE      0
#define GFX_DEBUG_LAYER0_MODE       1
#define GFX_DEBUG_LAYER1_MODE       2

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
uniform sampler2D   tileset;
uniform vec3        palette[PALETTE_SIZE];
uniform int         video_mode;
uniform int         debug_mode;


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
    vec2 addr = vec2((final_idx / SIZEOF_COLOR) / (TILESET_TEX_WIDTH + 0.0001), 0.0);
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
        return int(byte_value * 255);
    }
}


vec4 get_attr(vec2 flipped, out ivec2 offset_in_tile) {
    ivec2 pix_pos = ivec2(int(flipped.x), int(flipped.y));
    ivec2 tile_pos = ivec2(int(pix_pos.x) / TILE_WIDTH, int(pix_pos.y) / TILE_HEIGHT);
    int tile_idx = tile_pos.x + tile_pos.y * MAX_X;

    /* Get the address of the pixel to show within the 16x16 tile. Get it in one dimension */
    offset_in_tile.x = int(pix_pos.x) % TILE_WIDTH;
    offset_in_tile.y = int(pix_pos.y) % TILE_HEIGHT;
    int in_tile = offset_in_tile.x + (offset_in_tile.y * TILE_WIDTH);

    vec4 ret;
    if (debug_mode == GFX_DEBUG_TILESET_MODE) {
        ret = vec4(float(tile_idx), 0.0, 0.0, 0.0);
    } else {
        /* Added 0.1 to the divider to make sure we don't go beyond 1.0 */
        float float_idx = float(tile_idx) / (TILEMAP_ENTRIES + 0.1);
        ret = texture(tilemaps, vec2(float_idx, 1.0));
    }

    /* Replace unused B attribute with in_tile value */
    ret.b = float(in_tile);
    return ret;
}


vec4 gfx_mode(vec2 flipped, bool color_4bit, out int l0_icolor, out int l1_icolor) {
    ivec2 l0_offset;
    ivec2 l1_offset;
    vec4 attr_l0 = get_attr(flipped, l0_offset);
    vec4 attr_l1 = get_attr(flipped, l1_offset);

    if (color_4bit) {
        /* In 4-bit mode, the original index needs to be divided by 2 (so that 255 is the last tile)
         * of the first half */
        int l0_idx = int(attr_l0.r * (TILE_COUNT - 1));
        /* Get the lowest nibble of layer1 (attributes), if 1, offset the tile to the next tileset */
        int attr = int(attr_l0.g * 255);
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
        return vec4(palette[color + palette_idx], 1.0f);
    } else {
        /* 8-bit color mode */
        /* Get the two indexes */
        int l0_idx = int(attr_l0.r * (TILE_COUNT - 1));
        int l1_idx = int(attr_l1.g * (TILE_COUNT - 1));

        l0_icolor = color_from_idx(l0_idx, int(attr_l0.b), color_4bit);
        l1_icolor = color_from_idx(l1_idx, int(attr_l1.b), color_4bit);

        /* If layer1 color index is 0, count it as transparent */
        if (l1_icolor == 0) {
            return vec4(palette[l0_icolor], 0.0f);
        } else {
            return vec4(palette[l1_icolor], 1.0f);
        }
    }
}


void main() {
    int l0_icolor;
    int l1_icolor;
    bool color_4bit = video_mode == MODE_GFX_640_4BIT || video_mode == MODE_GFX_320_4BIT;

    /* Cooridnates are different in debug mode since we have the grid */
    ivec2 ipos = ivec2(gl_FragCoord.x, gl_FragCoord.y);
    ivec2 in_cell = ivec2(ipos.x % GRID_WIDTH, ipos.y % GRID_HEIGHT);
    ivec2 grid_size = ivec2(GRID_WIDTH, GRID_HEIGHT);
    /* Get the cell the current is in */
    ivec2 cell_idx = ipos / grid_size;
    /* Convert it to a screen position */
    ivec2 cell_pos = cell_idx * ivec2(TILE_WIDTH, TILE_HEIGHT);

    /* Highlight the "subscreens" (320x240 / 20x15 tiles) on the whole screen.
     * We must account for the grid size. This must NOT be done for the tileset mode. */
    if (debug_mode != GFX_DEBUG_TILESET_MODE &&
        (ipos.x % (20*GRID_WIDTH) == 0 || ipos.y % (15*GRID_HEIGHT) == 0))
    {
        finalColor = vec4(0.0, 0.65, 0.0, 1.0);
    } else if (debug_mode == GFX_DEBUG_TILESET_MODE && !color_4bit &&
        /* Only allow the first line of cell 17 to be drawn as red */
        (cell_idx.y > 16 || (cell_idx.y == 16 && in_cell.y > 0)))
    {
        /* If we are in 8-bit mode, we only have 256 tiles */
        /* Transparent pixel */
        finalColor = vec4(0.0, 0.0, 0.0, 0.0);
    } else if (in_cell.x == 0 || in_cell.y == 0) {
        finalColor = vec4(0.65, 0.0, 0.0, 1.0);
    } else {
        /* Get the coordinate of the pixel in cell, without the grid */
        in_cell -= ivec2(GRID_THICKNESS, GRID_THICKNESS);
        ivec2 pix_coords = cell_pos + in_cell;
        gfx_mode(vec2(pix_coords.x, pix_coords.y), color_4bit, l0_icolor, l1_icolor);
        if (debug_mode == GFX_DEBUG_TILESET_MODE) {
            finalColor = vec4(palette[l0_icolor], 1.0f);
        } else if (debug_mode == GFX_DEBUG_LAYER0_MODE)  {
            finalColor = vec4(palette[l0_icolor], 1.0f);
        } else if (l1_icolor != 0) {
            finalColor = vec4(palette[l1_icolor], 1.0f);
        } else {
            // Compute 2x2 subgrid index to show a grey grid
            int idx = (in_cell.y / 8) * 2 + (in_cell.x / 8);
            finalColor = (idx == 0 || idx == 3)
                ? vec4(0.5, 0.5, 0.5, 1.0)
                : vec4(0.8, 0.8, 0.8, 1.0);
        }
    }
}
