#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_tileset.h"


static void tileset_update_img(zvb_tileset_t* tileset, uint32_t addr, uint_fast8_t data);


void zvb_tileset_init(zvb_tileset_t* tileset)
{
    assert(tileset != NULL);

    /* Initialize both tilesets to 0 on boot (not reset) */
    memset(tileset->raw, 0, sizeof(tileset->raw));

    /* Create an empty bitmap image, to transmit the data faster to the GPU */
    const int size = (ZVB_TILESET_SIZE / sizeof(Color));
    tileset->img_tileset = GenImageColor(size, 1, BLACK);
    /* Set all the bytes to 0 */
    memset(tileset->img_tileset.data, 0, size);

    tileset->tex_tileset = LoadTextureFromImage(tileset->img_tileset);
    SetTextureFilter(tileset->tex_tileset, TEXTURE_FILTER_POINT);
    SetTextureWrap(tileset->tex_tileset, TEXTURE_WRAP_CLAMP);
    tileset->dirty = 0;
}


void zvb_tileset_write(zvb_tileset_t* tileset, uint32_t addr, uint8_t data)
{
    tileset->raw[addr] = data;
    tileset_update_img(tileset, addr, data);
}


uint8_t zvb_tileset_read(zvb_tileset_t* tileset, uint32_t addr)
{
    (void) tileset;
    (void) addr;
    return 0;
}


void zvb_tileset_update(zvb_tileset_t* tileset)
{
    if (tileset->dirty != 0) {
        UpdateTexture(tileset->tex_tileset, tileset->img_tileset.data);
        tileset->dirty = 0;
    }
}


/**
 * @brief Update the image with incoming byte
 */
static void tileset_update_img(zvb_tileset_t* tileset, uint32_t addr, uint_fast8_t data)
{
    _Static_assert(sizeof(Color) == 4, "Color should be 32-bit big");
    /* Pixels of the image can be access as an array directly */
    Color *pixels  = (Color*) (tileset->img_tileset.data);
    uint8_t *pixel = (uint8_t *) (pixels + addr / sizeof(Color));
    pixel[addr % sizeof(Color)] = data;
    tileset->dirty = 1;
}
