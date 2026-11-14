/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const uint16_t* pixels;
    int width;
    int height;
    size_t stride_bytes;
    uint64_t sequence;
} zvb_frame_t;

typedef struct {
    uint16_t* framebuffer;
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    uint16_t palette[256];
#endif
    int width;
    int height;
    uint64_t sequence;
#if ZVB_SCANLINE_RENDERING
    bool raster_frame;
#endif
} zvb_blitter_t;
