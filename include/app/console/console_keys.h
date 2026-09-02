/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "app/console/console.h"

/* Keyboard command handlers, defined in console_keys.c and referenced by the
 * console dispatch table (console.c). */
void console_key(zeal_t* machine, int argc, char** argv);
void console_tap(zeal_t* machine, int argc, char** argv);
