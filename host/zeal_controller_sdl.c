/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file zeal_controller_sdl.c
 * @brief SDL2 implementation of the controller seam.
 *
 * SDL's game controller API already speaks the mapping database format the seam
 * passes through, so zeal_controller_set_mappings() hands the text straight over.
 */
#include <SDL.h>
#include <stddef.h>
#include "host/zeal_controller.h"

static SDL_GameController *pads[ZEAL_CONTROLLER_COUNT];
static bool sdl_ready;

static const SDL_GameControllerButton buttons[ZEAL_CONTROLLER_BUTTON_COUNT] = {
    [ZEAL_CONTROLLER_BUTTON_FACE_DOWN] = SDL_CONTROLLER_BUTTON_A,
    [ZEAL_CONTROLLER_BUTTON_FACE_RIGHT] = SDL_CONTROLLER_BUTTON_B,
    [ZEAL_CONTROLLER_BUTTON_FACE_LEFT] = SDL_CONTROLLER_BUTTON_X,
    [ZEAL_CONTROLLER_BUTTON_FACE_UP] = SDL_CONTROLLER_BUTTON_Y,
    [ZEAL_CONTROLLER_BUTTON_DPAD_UP] = SDL_CONTROLLER_BUTTON_DPAD_UP,
    [ZEAL_CONTROLLER_BUTTON_DPAD_DOWN] = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    [ZEAL_CONTROLLER_BUTTON_DPAD_LEFT] = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    [ZEAL_CONTROLLER_BUTTON_DPAD_RIGHT] = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    [ZEAL_CONTROLLER_BUTTON_SELECT] = SDL_CONTROLLER_BUTTON_BACK,
    [ZEAL_CONTROLLER_BUTTON_START] = SDL_CONTROLLER_BUTTON_START,
    [ZEAL_CONTROLLER_BUTTON_SHOULDER_LEFT] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    [ZEAL_CONTROLLER_BUTTON_SHOULDER_RIGHT] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    /* SDL exposes the analog triggers as axes only, so the matching button entries
     * stay unset and zeal_controller_axis() reports them instead. */
};

static const SDL_GameControllerAxis axes[ZEAL_CONTROLLER_AXIS_COUNT] = {
    [ZEAL_CONTROLLER_AXIS_LEFT_X] = SDL_CONTROLLER_AXIS_LEFTX,
    [ZEAL_CONTROLLER_AXIS_LEFT_Y] = SDL_CONTROLLER_AXIS_LEFTY,
    [ZEAL_CONTROLLER_AXIS_RIGHT_X] = SDL_CONTROLLER_AXIS_RIGHTX,
    [ZEAL_CONTROLLER_AXIS_RIGHT_Y] = SDL_CONTROLLER_AXIS_RIGHTY,
    [ZEAL_CONTROLLER_AXIS_TRIGGER_LEFT] = SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    [ZEAL_CONTROLLER_AXIS_TRIGGER_RIGHT] = SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
};

static bool sdl_init_once(void)
{
    if (sdl_ready) {
        return true;
    }
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0) {
        return false;
    }
    sdl_ready = true;
    return true;
}

void zeal_controller_poll(void)
{
    if (!sdl_init_once()) {
        return;
    }
    SDL_PumpEvents();
    SDL_GameControllerUpdate();
    for (int i = 0; i < ZEAL_CONTROLLER_COUNT; i++) {
        if (pads[i] != NULL && !SDL_GameControllerGetAttached(pads[i])) {
            SDL_GameControllerClose(pads[i]);
            pads[i] = NULL;
        }
        if (pads[i] == NULL && SDL_IsGameController(i)) {
            pads[i] = SDL_GameControllerOpen(i);
        }
    }
}

bool zeal_controller_available(int index)
{
    return index >= 0 && index < ZEAL_CONTROLLER_COUNT && pads[index] != NULL;
}

const char *zeal_controller_name(int index)
{
    if (!zeal_controller_available(index)) {
        return NULL;
    }
    return SDL_GameControllerName(pads[index]);
}

bool zeal_controller_button_down(int index, zeal_controller_button_t button)
{
    if (!zeal_controller_available(index) || button < 0 || button >= ZEAL_CONTROLLER_BUTTON_COUNT) {
        return false;
    }
    if (buttons[button] == SDL_CONTROLLER_BUTTON_INVALID) {
        return false;
    }
    return SDL_GameControllerGetButton(pads[index], buttons[button]) != 0;
}

float zeal_controller_axis(int index, zeal_controller_axis_t axis)
{
    if (!zeal_controller_available(index) || axis < 0 || axis >= ZEAL_CONTROLLER_AXIS_COUNT) {
        return 0.0f;
    }
    const Sint16 value = SDL_GameControllerGetAxis(pads[index], axes[axis]);
    /* Triggers rest at the most negative value and only travel upwards. */
    if (axis == ZEAL_CONTROLLER_AXIS_TRIGGER_LEFT || axis == ZEAL_CONTROLLER_AXIS_TRIGGER_RIGHT) {
        return value <= 0 ? 0.0f : (float)value / 32767.0f;
    }
    return (float)value / 32767.0f;
}

void zeal_controller_set_mappings(const char *text)
{
    if (text == NULL || !sdl_init_once()) {
        return;
    }
    SDL_RWops *rw = SDL_RWFromConstMem(text, (int)SDL_strlen(text));
    if (rw != NULL) {
        /* The second argument asks SDL to close the RWops itself. */
        SDL_GameControllerAddMappingsFromRW(rw, 1);
    }
}
