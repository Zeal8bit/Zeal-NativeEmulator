/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>

/**
 * @brief Software blitter state.
 *
 * Renders every video mode into a plain CPU pixel buffer. There is no GPU resource
 * of any kind: the host presents the framebuffer directly, and the debugger reads it
 * without a readback.
 */
typedef struct {
    /* RGB565 framebuffer, 640x480 = 614 400 bytes. The final image for every mode. */
    uint16_t* framebuffer;
} zvb_blitter_t;
