#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_tilemap.h"

static void tilemap_update_img(zvb_tilemap_t* tilemap, int layer, uint32_t addr, uint_fast8_t data);


void zvb_tilemap_init(zvb_tilemap_t* tilemap)
{
    assert(tilemap != NULL);

    /* Initialize both tilemaps to 0 on boot (not reset) */
    memset(tilemap->raw_layer0, 0, sizeof(tilemap->raw_layer0));
    memset(tilemap->raw_layer1, 0, sizeof(tilemap->raw_layer1));

    /* Create an empty bitmap image, the goal is NOT to the have it represent the image
     * to show, the goal is to be able to provide both layers to the shader efficiently.
     * Moreover shaders cannot perform bitwise operations, so do it in this image/texture.
     * For each pixel i, the data is organized as follows:
     * R - Layer0[i]
     * G - Layer1[i]
     * B - (Layer1[i] >> 4) & 0xf (background)
     * A - (Layer1[i] >> 0) & 0xf (foreground)
     */
    tilemap->img_tilemap = GenImageColor(ZVB_TILEMAP_SIZE, 1, BLACK);
    /* Set all the bytes to 0 */
    memset(tilemap->img_tilemap.data, 0, sizeof(Color) * ZVB_TILEMAP_SIZE);

    tilemap->tex_tilemap = LoadTextureFromImage(tilemap->img_tilemap);
    tilemap->dirty = 0;
}


void zvb_tilemap_write(zvb_tilemap_t* tilemap, int layer, uint32_t addr, uint8_t data)
{
    if (layer == 0) {
        tilemap->raw_layer0[addr] = data;
    } else {
        tilemap->raw_layer1[addr] = data;
    }
    tilemap_update_img(tilemap, layer, addr, data);
}


uint8_t zvb_tilemap_read(zvb_tilemap_t* tilemap, int layer, uint32_t addr)
{
    (void) tilemap;
    (void) addr;
    (void) layer;
    return 0;
}


void zvb_tilemap_update(zvb_tilemap_t* tilemap)
{
    if (tilemap->dirty != 0) {
        UpdateTexture(tilemap->tex_tilemap, tilemap->img_tilemap.data);
        tilemap->dirty = 0;
    }
}


/**
 * @brief Update the image with incoming byte from a given layer
 */
static void tilemap_update_img(zvb_tilemap_t* tilemap, int layer, uint32_t addr, uint_fast8_t data)
{
    /* Pixels of the image can be access as an array directly */
    Color *pixels = (Color*) tilemap->img_tilemap.data;
    Color *pixel  = pixels + addr;

    if (layer == 0) {
        pixel->r = data;
    } else {
        pixel->g = data;
        pixel->b = (data >> 4) & 0xf;
        pixel->a = (data >> 0) & 0xf;
    }

    tilemap->dirty = 1;
}
