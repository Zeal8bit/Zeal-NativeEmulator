/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* SDL3 gamepads mapped onto SNES controllers. Host gamepads occupy stable
 * slots (0..SNES_GAMEPAD_COUNT-1) in connection order. */

#include <stdio.h>
#include <SDL3/SDL.h>
#include "controller.h"

/* SDL axis range is -32768..32767 */
#define AXIS_SCALE  (1.0f / 32767.0f)

static SDL_Gamepad* s_slots[SNES_GAMEPAD_COUNT];

static int slot_of(SDL_JoystickID id)
{
    for (int i = 0; i < SNES_GAMEPAD_COUNT; i++) {
        if (s_slots[i] && SDL_GetGamepadID(s_slots[i]) == id) return i;
    }
    return -1;
}

static void slot_open(SDL_JoystickID id)
{
    if (slot_of(id) >= 0) return;
    for (int i = 0; i < SNES_GAMEPAD_COUNT; i++) {
        if (s_slots[i] == NULL) {
            s_slots[i] = SDL_OpenGamepad(id);
            return;
        }
    }
}

void snes_controller_init(snes_controller_t* ctrl, uint8_t index)
{
    ctrl->index = index;
    ctrl->port = SNES_PORT_DETACHED;
    ctrl->attached = false;
}

void snes_controller_event(const SDL_Event* event)
{
    if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        slot_open(event->gdevice.which);
    } else if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        int slot = slot_of(event->gdevice.which);
        if (slot >= 0) {
            SDL_CloseGamepad(s_slots[slot]);
            s_slots[slot] = NULL;
        }
    }
}

void snes_controller_shutdown(void)
{
    for (int i = 0; i < SNES_GAMEPAD_COUNT; i++) {
        if (s_slots[i]) {
            SDL_CloseGamepad(s_slots[i]);
            s_slots[i] = NULL;
        }
    }
}

bool snes_controller_available(uint8_t index)
{
    return index < SNES_GAMEPAD_COUNT && s_slots[index] != NULL && SDL_GamepadConnected(s_slots[index]);
}

const char* snes_controller_name(uint8_t index)
{
    if (index >= SNES_GAMEPAD_COUNT || s_slots[index] == NULL) return NULL;
    const char* name = SDL_GetGamepadName(s_slots[index]);
    return name ? name : "Gamepad";
}

static bool button(SDL_Gamepad* pad, SDL_GamepadButton b)
{
    return SDL_GetGamepadButton(pad, b);
}

static float axis(SDL_Gamepad* pad, SDL_GamepadAxis a)
{
    return SDL_GetGamepadAxis(pad, a) * AXIS_SCALE;
}

uint16_t snes_controller_latch(snes_controller_t* ctrl)
{
    const int index = ctrl->index;

    if (!snes_controller_available(index)) {
        if (ctrl->attached) {
            printf("[SNES] Controller on port %d no longer available\n", ctrl->port + 1);
            ctrl->attached = false;
        }
        return 0xFFFF;
    }
    if (!ctrl->attached) {
        printf("[SNES] \"%s\" is now available\n", snes_controller_name(index));
        ctrl->attached = true;
    }
    uint16_t bits = 0xFFFF;  /* no buttons pressed (active low) */
    SDL_Gamepad* pad = s_slots[index];

    /* Positional face buttons, matching the SNES layout */
    if (button(pad, SDL_GAMEPAD_BUTTON_SOUTH)) bits &= ~(1 << SNES_BTN_B);
    if (button(pad, SDL_GAMEPAD_BUTTON_WEST))  bits &= ~(1 << SNES_BTN_Y);
    if (button(pad, SDL_GAMEPAD_BUTTON_EAST))  bits &= ~(1 << SNES_BTN_A);
    if (button(pad, SDL_GAMEPAD_BUTTON_NORTH)) bits &= ~(1 << SNES_BTN_X);

    /* D-Pad */
    if (button(pad, SDL_GAMEPAD_BUTTON_DPAD_UP))    bits &= ~(1 << SNES_BTN_UP);
    if (button(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))  bits &= ~(1 << SNES_BTN_DOWN);
    if (button(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))  bits &= ~(1 << SNES_BTN_LEFT);
    if (button(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) bits &= ~(1 << SNES_BTN_RIGHT);

    /* Left thumbstick as D-Pad */
    const float stick_x = axis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    const float stick_y = axis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    if (stick_y < -SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_UP);
    if (stick_y >  SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_DOWN);
    if (stick_x < -SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_LEFT);
    if (stick_x >  SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_RIGHT);

    /* Select/Start */
    if (button(pad, SDL_GAMEPAD_BUTTON_BACK))  bits &= ~(1 << SNES_BTN_SELECT);
    if (button(pad, SDL_GAMEPAD_BUTTON_START)) bits &= ~(1 << SNES_BTN_START);

    /* L/R: shoulders or triggers */
    if (button(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) ||
        axis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > SNES_TRIGGER_DEADZONE) {
        bits &= ~(1 << SNES_BTN_L);
    }
    if (button(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ||
        axis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > SNES_TRIGGER_DEADZONE) {
        bits &= ~(1 << SNES_BTN_R);
    }

    return bits;
}
