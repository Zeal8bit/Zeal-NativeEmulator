/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once


#ifdef __cplusplus
extern "C" {
#endif

void zvb_blitter_init(zvb_t* dev);

void zvb_blitter_prepare_render_text_mode(zvb_t* zvb);

void zvb_blitter_render_text_mode(zvb_t* zvb);

void zvb_blitter_prepare_render_gfx_mode(zvb_t* zvb);

void zvb_blitter_prepare_render_bitmap_mode(zvb_t* zvb);

void zvb_blitter_render_bitmap_mode(zvb_t* zvb);

void zvb_blitter_render_gfx_mode(zvb_t* zvb);

/* Present black at the maximum resolution (VGA output disabled). */
void zvb_blitter_fill_black(zvb_t* zvb);

#if ZVB_SCANLINE_RENDERING
void zvb_blitter_render_scanline(zvb_t* zvb, int scanline);
#endif

void zvb_blitter_deinit(zvb_t* dev);

#ifdef __cplusplus
} /* extern "C" */
#endif
