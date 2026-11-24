/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>

/* Run the desktop lifecycle: resolve images from the configuration, create the
 * machine, run the frontend main loop, save flash changes and deinitialize.
 * `frames` enables frame-paced execution. Returns the process exit code. */
int desktop_run(bool frames);
