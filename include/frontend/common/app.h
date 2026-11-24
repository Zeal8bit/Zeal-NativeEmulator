/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Frontend entry points shared by zeal-headless and zeal-native. Each platform
 * implements them in frontend/headless or frontend/sdl-fltk; the generic
 * lifecycle is orchestrated by desktop_run(). */
#pragma once
#include "hw/zeal.h"

/* Open the platform resources (window, SDL, ...) before creating the machine. */
void frontend_open(void);
/* Create the machine, load symbols/media and bind input and output.
 * Returns 0 on success, non-zero on failure. */
int frontend_init(zeal_t*, bool frames, bool debug);
/* Attach keyboard, gamepads and SNES ports to the machine. */
void frontend_bind_input(zeal_t*);
/* Attach audio and presentation. Returns 0 on success. */
int frontend_bind_output(zeal_t*);
/* Release keys held by the host, called after a machine reset. */
void frontend_reset_input(zeal_t*);
/* Run the main loop until the machine exits; returns the process exit code. */
int frontend_run(zeal_t*);
/* Detach input/output and close the platform resources. */
void frontend_close(zeal_t*);

/* frontend_close() followed by zeal_deinit(). */
void frontend_deinit(zeal_t*);
