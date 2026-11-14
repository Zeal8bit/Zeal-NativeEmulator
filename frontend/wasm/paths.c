/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stddef.h>
#include "utils/paths.h"

void get_executable_path(char* buffer, size_t size)
{
    if (size > 1) {
        buffer[0] = '/';
        buffer[1] = 0;
    }
}
