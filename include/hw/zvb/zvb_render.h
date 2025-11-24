/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Abstarct API that must be implemented by the renderers
 */


typedef struct zvb_render_t zvb_render_t;

void zvb_render_init(zvb_render_t* render, zvb_t* dev);

void zvb_render_text_mode(zvb_render_t* render, zvb_t* zvb);

void zvb_render_debug_text_mode(zvb_render_t* render, zvb_t* zvb);

void zvb_render_bitmap_mode(zvb_render_t* render, zvb_t* zvb);

void zvb_render_gfx_mode(zvb_render_t* render, zvb_t* zvb);

void zvb_render_debug_gfx_mode(zvb_render_t* render, zvb_t* zvb);
