/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file zeal_controller.h
 * @brief Game controller seam: what the emulator needs from a host gamepad.
 *
 * This is a compile-time seam like include/host/zeal_host.h. Exactly one backend is
 * linked into a binary, so the emulator never includes a gamepad library:
 *
 *   - host/zeal_controller_raylib.c -> Raylib's gamepad support (WebAssembly)
 *   - host/zeal_controller_null.c   -> no controllers
 *
 * The button and axis names are deliberately generic (face, d-pad, shoulder,
 * trigger) rather than copied from any one library, so a new backend (SDL, evdev,
 * ...) can map its own layout onto them without touching the SNES adapter.
 */
#ifndef ZEAL_CONTROLLER_H
#define ZEAL_CONTROLLER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Number of controllers the emulator can expose to the SNES adapter.
 */
#define ZEAL_CONTROLLER_COUNT 4

/**
 * @brief Buttons the emulator reads.
 *
 * Face buttons are named by position, not by letter: a gamepad's bottom face button
 * is FACE_DOWN whichever vendor label it carries.
 */
typedef enum {
    ZEAL_CONTROLLER_BUTTON_FACE_DOWN = 0, /* bottom face button */
    ZEAL_CONTROLLER_BUTTON_FACE_RIGHT,    /* right face button */
    ZEAL_CONTROLLER_BUTTON_FACE_LEFT,     /* left face button */
    ZEAL_CONTROLLER_BUTTON_FACE_UP,       /* top face button */
    ZEAL_CONTROLLER_BUTTON_DPAD_UP,
    ZEAL_CONTROLLER_BUTTON_DPAD_DOWN,
    ZEAL_CONTROLLER_BUTTON_DPAD_LEFT,
    ZEAL_CONTROLLER_BUTTON_DPAD_RIGHT,
    ZEAL_CONTROLLER_BUTTON_SELECT,
    ZEAL_CONTROLLER_BUTTON_START,
    ZEAL_CONTROLLER_BUTTON_SHOULDER_LEFT,  /* upper left shoulder */
    ZEAL_CONTROLLER_BUTTON_SHOULDER_RIGHT, /* upper right shoulder */
    ZEAL_CONTROLLER_BUTTON_TRIGGER_LEFT,   /* lower left trigger */
    ZEAL_CONTROLLER_BUTTON_TRIGGER_RIGHT,  /* lower right trigger */
    ZEAL_CONTROLLER_BUTTON_COUNT
} zeal_controller_button_t;

typedef enum {
    ZEAL_CONTROLLER_AXIS_LEFT_X = 0,
    ZEAL_CONTROLLER_AXIS_LEFT_Y,
    ZEAL_CONTROLLER_AXIS_RIGHT_X,
    ZEAL_CONTROLLER_AXIS_RIGHT_Y,
    ZEAL_CONTROLLER_AXIS_TRIGGER_LEFT,
    ZEAL_CONTROLLER_AXIS_TRIGGER_RIGHT,
    ZEAL_CONTROLLER_AXIS_COUNT
} zeal_controller_axis_t;

/**
 * @brief Refresh controller state and notice hotplug.
 *
 * Backends that cache device state need this once per frame; SDL does, Raylib reads
 * its gamepads on demand and only has to do nothing. Cheap enough to call often but
 * not free, so call it once per frame rather than per query.
 */
void zeal_controller_poll(void);

/**
 * @brief True when a controller is plugged in at this index.
 *
 * @param index 0 to ZEAL_CONTROLLER_COUNT - 1.
 */
bool zeal_controller_available(int index);

/**
 * @brief Human-readable controller name, or NULL when the index has no controller.
 *        The returned string is owned by the backend.
 */
const char *zeal_controller_name(int index);

/**
 * @brief True while the given button is held. False for an absent controller.
 */
bool zeal_controller_button_down(int index, zeal_controller_button_t button);

/**
 * @brief Axis position, -1.0 to 1.0 for sticks and 0.0 to 1.0 for triggers, or 0.0
 *        for an absent controller.
 */
float zeal_controller_axis(int index, zeal_controller_axis_t axis);

/**
 * @brief Replace the button mapping database, in SDL's gamecontrollerdb text format.
 *
 * Implementations without a mapping database may ignore this. The text is copied, so
 * the caller keeps ownership of @p text.
 */
void zeal_controller_set_mappings(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* ZEAL_CONTROLLER_H */
