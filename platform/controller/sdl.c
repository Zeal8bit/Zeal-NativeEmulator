/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file sdl.c
 * @brief SDL2 implementation of the controller seam.
 *
 * SDL's game controller API already speaks the mapping database format the seam
 * passes through, so controller_set_mappings() hands the text straight over.
 */
#include <SDL.h>
#include <stddef.h>
#include "platform/controller.h"

static SDL_GameController *pads[CONTROLLER_COUNT];
static bool sdl_ready;

static const SDL_GameControllerButton buttons[CONTROLLER_BUTTON_COUNT] = {
    [CONTROLLER_BUTTON_FACE_DOWN] = SDL_CONTROLLER_BUTTON_A,
    [CONTROLLER_BUTTON_FACE_RIGHT] = SDL_CONTROLLER_BUTTON_B,
    [CONTROLLER_BUTTON_FACE_LEFT] = SDL_CONTROLLER_BUTTON_X,
    [CONTROLLER_BUTTON_FACE_UP] = SDL_CONTROLLER_BUTTON_Y,
    [CONTROLLER_BUTTON_DPAD_UP] = SDL_CONTROLLER_BUTTON_DPAD_UP,
    [CONTROLLER_BUTTON_DPAD_DOWN] = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    [CONTROLLER_BUTTON_DPAD_LEFT] = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    [CONTROLLER_BUTTON_DPAD_RIGHT] = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    [CONTROLLER_BUTTON_SELECT] = SDL_CONTROLLER_BUTTON_BACK,
    [CONTROLLER_BUTTON_START] = SDL_CONTROLLER_BUTTON_START,
    [CONTROLLER_BUTTON_SHOULDER_LEFT] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    [CONTROLLER_BUTTON_SHOULDER_RIGHT] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    /* SDL exposes the analog triggers as axes only, so the matching button entries
     * stay unset and controller_axis() reports them instead. */
};

static const SDL_GameControllerAxis axes[CONTROLLER_AXIS_COUNT] = {
    [CONTROLLER_AXIS_LEFT_X] = SDL_CONTROLLER_AXIS_LEFTX,
    [CONTROLLER_AXIS_LEFT_Y] = SDL_CONTROLLER_AXIS_LEFTY,
    [CONTROLLER_AXIS_RIGHT_X] = SDL_CONTROLLER_AXIS_RIGHTX,
    [CONTROLLER_AXIS_RIGHT_Y] = SDL_CONTROLLER_AXIS_RIGHTY,
    [CONTROLLER_AXIS_TRIGGER_LEFT] = SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    [CONTROLLER_AXIS_TRIGGER_RIGHT] = SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
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

void controller_poll(void)
{
    if (!sdl_init_once()) {
        return;
    }
    SDL_PumpEvents();
    SDL_GameControllerUpdate();
    for (int i = 0; i < CONTROLLER_COUNT; i++) {
        if (pads[i] != NULL && !SDL_GameControllerGetAttached(pads[i])) {
            SDL_GameControllerClose(pads[i]);
            pads[i] = NULL;
        }
        if (pads[i] == NULL && SDL_IsGameController(i)) {
            pads[i] = SDL_GameControllerOpen(i);
        }
    }
}

bool controller_available(int index)
{
    return index >= 0 && index < CONTROLLER_COUNT && pads[index] != NULL;
}

const char *controller_name(int index)
{
    if (!controller_available(index)) {
        return NULL;
    }
    return SDL_GameControllerName(pads[index]);
}

bool controller_button_down(int index, controller_button_t button)
{
    if (!controller_available(index) || button < 0 || button >= CONTROLLER_BUTTON_COUNT) {
        return false;
    }
    if (buttons[button] == SDL_CONTROLLER_BUTTON_INVALID) {
        return false;
    }
    return SDL_GameControllerGetButton(pads[index], buttons[button]) != 0;
}

float controller_axis(int index, controller_axis_t axis)
{
    if (!controller_available(index) || axis < 0 || axis >= CONTROLLER_AXIS_COUNT) {
        return 0.0f;
    }
    const Sint16 value = SDL_GameControllerGetAxis(pads[index], axes[axis]);
    /* Triggers rest at the most negative value and only travel upwards. */
    if (axis == CONTROLLER_AXIS_TRIGGER_LEFT || axis == CONTROLLER_AXIS_TRIGGER_RIGHT) {
        return value <= 0 ? 0.0f : (float)value / 32767.0f;
    }
    return (float)value / 32767.0f;
}

void controller_set_mappings(const char *text)
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
