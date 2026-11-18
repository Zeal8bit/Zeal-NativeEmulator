/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* UART console capture for the Console view (debug/console.c). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FZ_CONSOLE_LINES     4096
#define FZ_CONSOLE_LINE_MAX  160   /* longer lines wrap */

typedef struct {
    const char* text;       /* not NUL-terminated; valid until the next byte */
    int         len;
    uint64_t    time_ns;    /* emulated time of the first byte */
} fz_console_line_t;

/** Changes whenever the buffer changes (cheap polling). */
uint64_t fz_console_serial(void);
/** Absolute number of the oldest kept line; lines are numbered from 0. */
uint64_t fz_console_first(void);
/** Lines kept, including the unterminated last one. */
int      fz_console_count(void);
uint64_t fz_console_bytes(void);
/** Line by absolute number; false if no longer (or not yet) kept. */
bool     fz_console_line(uint64_t number, fz_console_line_t* out);
void     fz_console_clear(void);

#ifdef __cplusplus
}
#endif
