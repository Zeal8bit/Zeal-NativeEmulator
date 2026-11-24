/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* Host services owned by the frontend and outliving the machine. Callbacks
 * are invoked synchronously on the emulation thread; the frontend owns stream
 * selection, verbosity, blocking input and flushing. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* userdata;
    void (*log)(void*, bool error, int verbosity, const char* format, va_list);
    int (*read_byte)(void*); /* -1 on EOF/error; otherwise an unsigned byte */
    void (*uart_byte)(void*, uint8_t);
} hw_host_t;

static inline void hw_log(const hw_host_t* host, bool error, int verbosity, const char* format, ...)
{
    if (host == NULL || host->log == NULL) {
        return;
    }
    va_list args;
    va_start(args, format);
    host->log(host->userdata, error, verbosity, format, args);
    va_end(args);
}

/* Log helpers: errors go to the frontend's error stream, info to the regular
 * output. Verbose diagnostics are filtered by the frontend; level 0 is always
 * shown. */
#define hw_log_error(host, ...)             hw_log((host), true, 0, __VA_ARGS__)
#define hw_log_info(host, ...)              hw_log((host), false, 0, __VA_ARGS__)
#define hw_log_verbose(host, level, ...)    hw_log((host), false, (level), __VA_ARGS__)

#ifdef __cplusplus
} /* extern "C" */
#endif
