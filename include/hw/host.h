/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

/* Borrowed callbacks: invoked synchronously on the emulation thread.
 * Frontends own stream selection, verbosity, blocking input, and flushing. */
typedef struct {
    void* userdata;
    void (*log)(void*, bool error, int verbosity, const char* format, va_list);
    int (*read_byte)(void*); /* -1 on EOF/error; otherwise an unsigned byte */
    void (*uart_byte)(void*, uint8_t);
} hw_host_t;

static inline void hw_log(const hw_host_t* host, bool error, int verbosity, const char* format, ...)
{
    if (!host || !host->log) return;
    va_list args;
    va_start(args, format);
    host->log(host->userdata, error, verbosity, format, args);
    va_end(args);
}
