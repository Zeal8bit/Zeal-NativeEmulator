/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include "mouse.h"
#include "video.h"
#include "utils/notif.h"

void snes_mouse_init(snes_mouse_t* mouse)
{
    *mouse = (snes_mouse_t){.attached = true, .delta_scale = SNES_MOUSE_DELTA_SCALE};
}

void snes_mouse_detach(snes_mouse_t* mouse)
{
    mouse->attached = false;
    mouse->dx = mouse->dy = 0;
    if (wasm_window) {
        SDL_SetWindowRelativeMouseMode(wasm_window, false);
    }
}

void snes_mouse_reset_scale(snes_mouse_t* mouse)
{
    mouse->delta_scale = SNES_MOUSE_DELTA_SCALE;
    notif_show("SNES Mouse Scale: x%.2f", mouse->delta_scale);
}

void snes_mouse_event(snes_mouse_t* mouse, SDL_Event* event)
{
    if (!mouse->attached) {
        return;
    }
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        SDL_ConvertEventToRenderCoordinates(wasm_renderer, event);
        mouse->dx += event->motion.xrel;
        mouse->dy += event->motion.yrel;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_MIDDLE) {
            SDL_SetWindowRelativeMouseMode(wasm_window, !SDL_GetWindowRelativeMouseMode(wasm_window));
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        mouse->delta_scale = SDL_clamp(mouse->delta_scale + event->wheel.y * SNES_MOUSE_DELTA_SCALE_STEP,
                                       SNES_MOUSE_DELTA_SCALE_MIN, SNES_MOUSE_DELTA_SCALE_MAX);
        notif_show("SNES Mouse Speed: x%.2f", mouse->delta_scale);
        break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        mouse->dx = mouse->dy = 0;
        break;
    }
}

uint32_t snes_mouse_latch(snes_mouse_t* mouse)
{
    float dx = mouse->dx * mouse->delta_scale;
    float dy = mouse->dy * mouse->delta_scale;
    mouse->dx = mouse->dy = 0;
    SDL_MouseButtonFlags buttons = SDL_GetMouseFocus() ? SDL_GetMouseState(NULL, NULL) : 0;
    return snes_mouse_packet((uint8_t)fminf(roundf(fabsf(dx)), SNES_MOUSE_MAG_MASK), dx < 0,
                             (uint8_t)fminf(roundf(fabsf(dy)), SNES_MOUSE_MAG_MASK), dy < 0,
                             SNES_MOUSE_DEFAULT_SPEED, buttons & SDL_BUTTON_LMASK, buttons & SDL_BUTTON_RMASK);
}
