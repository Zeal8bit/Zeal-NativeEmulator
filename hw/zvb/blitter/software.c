/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "hw/zvb/zvb.h"
#include "utils/log.h"


/**
 * @brief Calculate the size (width or height) counting a grid
 */
#define SIZE_WITH_GRID(TILESIZE, TILECOUNT)    ((((TILESIZE)+1)*TILECOUNT)+1)

void zvb_blitter_init(zvb_t* dev)
{
}


/**
 * @brief Render the screen in text mode (80x40 and 40x20)
 */
void zvb_blitter_prepare_render_text_mode(zvb_t* zvb)
{
}


void zvb_blitter_render_text_mode(zvb_t* zvb)
{
}


/**
 * @brief Render the debug textures when we are in text mode.
 * The `zvb_render` function must be called first.
 */
void zvb_blitter_render_debug_text_mode(zvb_t* zvb)
{
}


void zvb_blitter_prepare_render_gfx_mode(zvb_t* zvb)
{
}

void zvb_blitter_prepare_render_bitmap_mode(zvb_t* zvb)
{
}

/**
 * @brief Render the screen in bitmap mode
 */
void zvb_blitter_render_bitmap_mode(zvb_t* zvb)
{
}


/**
 * @brief Render the screen in graphics mode
 */
void zvb_blitter_render_gfx_mode(zvb_t* zvb)
{
}


void zvb_blitter_render_debug_gfx_mode(zvb_t* zvb)
{
}

void zvb_blitter_deinit(zvb_t* zvb)
{
}
