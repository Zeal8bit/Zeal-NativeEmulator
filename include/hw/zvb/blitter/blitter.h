/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

struct zvb_t;
struct zvb_blitter_t;

void zvb_blitter_init(zvb_t* dev);

void zvb_blitter_prepare_render_text_mode(zvb_t* zvb);

void zvb_blitter_render_text_mode(zvb_t* zvb);

void zvb_blitter_prepare_render_gfx_mode(zvb_t* zvb);

void zvb_blitter_prepare_render_bitmap_mode(zvb_t* zvb);

void zvb_blitter_render_bitmap_mode(zvb_t* zvb);

void zvb_blitter_render_gfx_mode(zvb_t* zvb);

#if ZVB_BLITTER_SHADER || ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
void zvb_blitter_render_scanline(zvb_t* zvb, int scanline);
#endif

void zvb_blitter_deinit(zvb_t* dev);
