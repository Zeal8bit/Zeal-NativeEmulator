/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file null.c
 * @brief Controller seam with no controllers attached.
 *
 * Selecting this backend lets the emulator build and run without any gamepad
 * library; the SNES adapter simply reports every port as having no controller.
 */
#include <stddef.h>
#include "platform/controller.h"

void controller_poll(void) {}

bool controller_available(int index)
{
    (void)index;
    return false;
}

const char *controller_name(int index)
{
    (void)index;
    return NULL;
}

bool controller_button_down(int index, controller_button_t button)
{
    (void)index;
    (void)button;
    return false;
}

float controller_axis(int index, controller_axis_t axis)
{
    (void)index;
    (void)axis;
    return 0.0f;
}

void controller_set_mappings(const char *text) { (void)text; }
