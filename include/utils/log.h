/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <errno.h>


#include <stdio.h>

#define LOG_FD_INPUT            0
#define LOG_FD_OUTPUT           1
#define LOG_FD_ERROR            2

#define log_printf(...)         printf(__VA_ARGS__)
#define log_err_printf(...)     fprintf(stderr, __VA_ARGS__)
#define log_perror(fmt, ...) \
    fprintf(stderr, fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
#define log_flush()             fflush(stdout)
