/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file controller.h
 * @brief Game controller seam: what the emulator needs from a gamepad.
 *
 * This is a compile-time seam like include/platform/display.h. Exactly one backend is
 * linked into a binary, so the emulator never includes a gamepad library:
 *
 *   - platform/controller/sdl.c    -> SDL2's gamepad support (desktop)
 *   - platform/controller/raylib.c -> Raylib's gamepad support (WebAssembly)
 *   - platform/controller/null.c   -> no controllers
 *
 * The button and axis names are deliberately generic (face, d-pad, shoulder,
 * trigger) rather than copied from any one library, so a new backend (SDL, evdev,
 * ...) can map its own layout onto them without touching the SNES adapter.
 */
#ifndef PLATFORM_CONTROLLER_H
#define PLATFORM_CONTROLLER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Number of controllers the emulator can expose to the SNES adapter.
 */
#define CONTROLLER_COUNT 4

/**
 * @brief Buttons the emulator reads.
 *
 * Face buttons are named by position, not by letter: a gamepad's bottom face button
 * is FACE_DOWN whichever vendor label it carries.
 */
typedef enum {
    CONTROLLER_BUTTON_FACE_DOWN = 0, /* bottom face button */
    CONTROLLER_BUTTON_FACE_RIGHT,    /* right face button */
    CONTROLLER_BUTTON_FACE_LEFT,     /* left face button */
    CONTROLLER_BUTTON_FACE_UP,       /* top face button */
    CONTROLLER_BUTTON_DPAD_UP,
    CONTROLLER_BUTTON_DPAD_DOWN,
    CONTROLLER_BUTTON_DPAD_LEFT,
    CONTROLLER_BUTTON_DPAD_RIGHT,
    CONTROLLER_BUTTON_SELECT,
    CONTROLLER_BUTTON_START,
    CONTROLLER_BUTTON_SHOULDER_LEFT,  /* upper left shoulder */
    CONTROLLER_BUTTON_SHOULDER_RIGHT, /* upper right shoulder */
    CONTROLLER_BUTTON_TRIGGER_LEFT,   /* lower left trigger */
    CONTROLLER_BUTTON_TRIGGER_RIGHT,  /* lower right trigger */
    CONTROLLER_BUTTON_COUNT
} controller_button_t;

typedef enum {
    CONTROLLER_AXIS_LEFT_X = 0,
    CONTROLLER_AXIS_LEFT_Y,
    CONTROLLER_AXIS_RIGHT_X,
    CONTROLLER_AXIS_RIGHT_Y,
    CONTROLLER_AXIS_TRIGGER_LEFT,
    CONTROLLER_AXIS_TRIGGER_RIGHT,
    CONTROLLER_AXIS_COUNT
} controller_axis_t;

/**
 * @brief Refresh controller state and notice hotplug.
 *
 * Backends that cache device state need this once per frame; SDL does, Raylib reads
 * its gamepads on demand and only has to do nothing. Cheap enough to call often but
 * not free, so call it once per frame rather than per query.
 */
void controller_poll(void);

/**
 * @brief True when a controller is plugged in at this index.
 *
 * @param index 0 to CONTROLLER_COUNT - 1.
 */
bool controller_available(int index);

/**
 * @brief Human-readable controller name, or NULL when the index has no controller.
 *        The returned string is owned by the backend.
 */
const char *controller_name(int index);

/**
 * @brief True while the given button is held. False for an absent controller.
 */
bool controller_button_down(int index, controller_button_t button);

/**
 * @brief Axis position, -1.0 to 1.0 for sticks and 0.0 to 1.0 for triggers, or 0.0
 *        for an absent controller.
 */
float controller_axis(int index, controller_axis_t axis);

/**
 * @brief Replace the button mapping database, in SDL's gamecontrollerdb text format.
 *
 * Implementations without a mapping database may ignore this. The text is copied, so
 * the caller keeps ownership of @p text.
 */
void controller_set_mappings(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_CONTROLLER_H */
