/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* UART console capture for the Console view (bridge/console.h): every byte
 * the machine sends on its serial port, split into timestamped lines. */

#include <stdio.h>
#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/bridge/console.h"
#include "frontend/common/host.h"

typedef struct {
    char     text[FZ_CONSOLE_LINE_MAX];
    int      len;
    uint64_t time_ns;
} console_line_t;

static console_line_t s_lines[FZ_CONSOLE_LINES];
static uint64_t s_first;      /* oldest kept line */
static uint64_t s_cur;        /* line being written */
static bool     s_started;    /* the current line received a byte */
static uint64_t s_serial;
static uint64_t s_bytes;

static console_line_t* cur_line(void)
{
    return &s_lines[s_cur % FZ_CONSOLE_LINES];
}

static void console_newline(void)
{
    s_cur++;
    if (s_cur - s_first >= FZ_CONSOLE_LINES) s_first = s_cur - FZ_CONSOLE_LINES + 1;
    cur_line()->len = 0;
    s_started = false;
}

static void console_put(char c)
{
    console_line_t* line = cur_line();
    if (line->len >= FZ_CONSOLE_LINE_MAX) {
        console_newline();
        line = cur_line();
        line->time_ns = fz_emulated_ns();
        s_started = true;
    }
    line->text[line->len++] = c;
}

static void console_tap(void* userdata, uint8_t byte)
{
    (void) userdata;
    s_bytes++;
    s_serial++;
    if (byte == '\r') return;
    if (!s_started) {
        cur_line()->time_ns = fz_emulated_ns();
        s_started = true;
    }
    console_line_t* line = cur_line();
    switch (byte) {
        case '\n':
            console_newline();
            break;
        case '\b':
        case 0x7F:
            if (line->len > 0) line->len--;
            break;
        case '\t':
            do console_put(' '); while (cur_line()->len % 8 != 0);
            break;
        default:
            console_put((char) byte);
            break;
    }
}

void fltk_console_bind(void)
{
    frontend_uart_tap = console_tap;
    frontend_uart_tap_userdata = NULL;
}

uint64_t fz_console_serial(void)
{
    return s_serial;
}

uint64_t fz_console_first(void)
{
    return s_first;
}

int fz_console_count(void)
{
    return (int) (s_cur - s_first) + (cur_line()->len > 0 ? 1 : 0);
}

uint64_t fz_console_bytes(void)
{
    return s_bytes;
}

bool fz_console_line(uint64_t number, fz_console_line_t* out)
{
    if (number < s_first || number > s_cur) return false;
    const console_line_t* line = &s_lines[number % FZ_CONSOLE_LINES];
    out->text = line->text;
    out->len = line->len;
    out->time_ns = line->time_ns;
    return true;
}

void fz_console_clear(void)
{
    s_first = s_cur = 0;
    s_lines[0].len = 0;
    s_started = false;
    s_serial++;
}
