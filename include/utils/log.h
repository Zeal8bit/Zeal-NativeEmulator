/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <errno.h>


#include <stdio.h>

#define log_printf(...)         printf(__VA_ARGS__)
#define log_err_printf(...)     fprintf(stderr, __VA_ARGS__)
#define log_perror(fmt, ...) \
    fprintf(stderr, fmt ": %s", ##__VA_ARGS__, strerror(errno))
