/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file zeal_controller_null.c
 * @brief Controller seam with no controllers attached.
 *
 * Selecting this backend lets the emulator build and run without any gamepad
 * library; the SNES adapter simply reports every port as having no host controller.
 */
#include <stddef.h>
#include "host/zeal_controller.h"

void zeal_controller_poll(void) {}

bool zeal_controller_available(int index)
{
    (void)index;
    return false;
}

const char *zeal_controller_name(int index)
{
    (void)index;
    return NULL;
}

bool zeal_controller_button_down(int index, zeal_controller_button_t button)
{
    (void)index;
    (void)button;
    return false;
}

float zeal_controller_axis(int index, zeal_controller_axis_t axis)
{
    (void)index;
    (void)axis;
    return 0.0f;
}

void zeal_controller_set_mappings(const char *text) { (void)text; }
