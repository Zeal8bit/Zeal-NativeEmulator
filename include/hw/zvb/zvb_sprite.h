#pragma once

#include <stdint.h>

/**
 * @brief Define the sprite organization as they are in the hardware
 */
typedef struct {
    uint16_t y;
    uint16_t x;
    /* Sprite's flag can be read as a byte or as a bitmap */
    union {
        struct {
            uint8_t tile_number : 8;
            uint8_t tileset_idx : 1;
            uint8_t behind_fg : 1;
            uint8_t flip_y : 1;
            uint8_t flip_x : 1;
            uint8_t palette : 4;
        } bitmap;
        uint16_t raw;
    } flags;
    uint16_t padding;
} zvb_sprite_t;

_Static_assert(sizeof(zvb_sprite_t) == 8, "Sprite structure must have a size of 8 bytes");
