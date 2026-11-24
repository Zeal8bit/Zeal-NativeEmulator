/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <SDL3/SDL.h>

#include "snes_adapter.h"

void snes_mouse_init(snes_mouse_t* mouse);
void snes_mouse_detach(snes_mouse_t* mouse);
void snes_mouse_reset_scale(snes_mouse_t* mouse);
void snes_mouse_event(snes_mouse_t* mouse, SDL_Event* event);
uint32_t snes_mouse_latch(snes_mouse_t* mouse);
