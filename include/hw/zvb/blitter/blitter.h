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

void zvb_blitter_render_debug_text_mode(zvb_t* zvb);

void zvb_blitter_prepare_render_gfx_mode(zvb_t* zvb);

void zvb_blitter_prepare_render_bitmap_mode(zvb_t* zvb);

void zvb_blitter_render_bitmap_mode(zvb_t* zvb);

void zvb_blitter_render_gfx_mode(zvb_t* zvb);

void zvb_blitter_render_debug_gfx_mode(zvb_t* zvb);

void zvb_blitter_deinit(zvb_t* dev);
