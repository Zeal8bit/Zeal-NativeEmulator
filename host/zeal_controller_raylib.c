/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file zeal_controller_raylib.c
 * @brief Raylib implementation of the controller seam.
 */
#include "raylib.h"
#include <stddef.h>
#include "host/zeal_controller.h"

static const int buttons[ZEAL_CONTROLLER_BUTTON_COUNT] = {
    [ZEAL_CONTROLLER_BUTTON_FACE_DOWN] = GAMEPAD_BUTTON_RIGHT_FACE_DOWN,
    [ZEAL_CONTROLLER_BUTTON_FACE_RIGHT] = GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
    [ZEAL_CONTROLLER_BUTTON_FACE_LEFT] = GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
    [ZEAL_CONTROLLER_BUTTON_FACE_UP] = GAMEPAD_BUTTON_RIGHT_FACE_UP,
    [ZEAL_CONTROLLER_BUTTON_DPAD_UP] = GAMEPAD_BUTTON_LEFT_FACE_UP,
    [ZEAL_CONTROLLER_BUTTON_DPAD_DOWN] = GAMEPAD_BUTTON_LEFT_FACE_DOWN,
    [ZEAL_CONTROLLER_BUTTON_DPAD_LEFT] = GAMEPAD_BUTTON_LEFT_FACE_LEFT,
    [ZEAL_CONTROLLER_BUTTON_DPAD_RIGHT] = GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
    [ZEAL_CONTROLLER_BUTTON_SELECT] = GAMEPAD_BUTTON_MIDDLE_LEFT,
    [ZEAL_CONTROLLER_BUTTON_START] = GAMEPAD_BUTTON_MIDDLE_RIGHT,
    [ZEAL_CONTROLLER_BUTTON_SHOULDER_LEFT] = GAMEPAD_BUTTON_LEFT_TRIGGER_1,
    [ZEAL_CONTROLLER_BUTTON_SHOULDER_RIGHT] = GAMEPAD_BUTTON_RIGHT_TRIGGER_1,
    [ZEAL_CONTROLLER_BUTTON_TRIGGER_LEFT] = GAMEPAD_BUTTON_LEFT_TRIGGER_2,
    [ZEAL_CONTROLLER_BUTTON_TRIGGER_RIGHT] = GAMEPAD_BUTTON_RIGHT_TRIGGER_2,
};

static const int axes[ZEAL_CONTROLLER_AXIS_COUNT] = {
    [ZEAL_CONTROLLER_AXIS_LEFT_X] = GAMEPAD_AXIS_LEFT_X,
    [ZEAL_CONTROLLER_AXIS_LEFT_Y] = GAMEPAD_AXIS_LEFT_Y,
    [ZEAL_CONTROLLER_AXIS_RIGHT_X] = GAMEPAD_AXIS_RIGHT_X,
    [ZEAL_CONTROLLER_AXIS_RIGHT_Y] = GAMEPAD_AXIS_RIGHT_Y,
    [ZEAL_CONTROLLER_AXIS_TRIGGER_LEFT] = GAMEPAD_AXIS_LEFT_TRIGGER,
    [ZEAL_CONTROLLER_AXIS_TRIGGER_RIGHT] = GAMEPAD_AXIS_RIGHT_TRIGGER,
};

void zeal_controller_poll(void) {}

bool zeal_controller_available(int index)
{
    return index >= 0 && index < ZEAL_CONTROLLER_COUNT && IsGamepadAvailable(index);
}

const char *zeal_controller_name(int index)
{
    return zeal_controller_available(index) ? GetGamepadName(index) : NULL;
}

bool zeal_controller_button_down(int index, zeal_controller_button_t button)
{
    if (!zeal_controller_available(index) || button < 0 || button >= ZEAL_CONTROLLER_BUTTON_COUNT) {
        return false;
    }
    return IsGamepadButtonDown(index, buttons[button]);
}

float zeal_controller_axis(int index, zeal_controller_axis_t axis)
{
    if (!zeal_controller_available(index) || axis < 0 || axis >= ZEAL_CONTROLLER_AXIS_COUNT) {
        return 0.0f;
    }
    return GetGamepadAxisMovement(index, axes[axis]);
}

void zeal_controller_set_mappings(const char *text)
{
    if (text != NULL) {
        SetGamepadMappings(text);
    }
}
