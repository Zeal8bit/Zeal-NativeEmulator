/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <SDL3/SDL.h>
#include "hw/zvb/zvb.h"

extern SDL_Window* wasm_window;
extern SDL_Renderer* wasm_renderer;
bool wasm_video_init(zvb_t* zvb);
bool wasm_video_draw(zvb_t* zvb, bool show_fps);
void wasm_video_deinit(void);
