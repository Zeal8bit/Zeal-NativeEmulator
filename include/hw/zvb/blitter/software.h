/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include "raylib.h"

/**
 * @brief Software blitter state.
 *
 * Renders all video modes on CPU into a pixel buffer, then uploads to GPU
 * via UpdateTexture once per frame.  No GPU shaders, no FBO — plain RAM.
 */
typedef struct {
    /* RGB565 framebuffer, 640×480 = 614 400 bytes */
    uint16_t*     framebuffer;
    /* Raylib Image wrapping framebuffer for UpdateTexture */
    Image         fb_image;
    /* CPU-rendered texture (RGB565), uploaded from framebuffer each frame */
    Texture2D     output_texture;
    /* Output render target, blitted from output_texture for debugger compatibility */
    RenderTexture main_texture;
} zvb_blitter_t;
