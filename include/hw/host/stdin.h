/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#if !defined(PLATFORM_WEB) && !defined(_WIN32)
#include <termios.h>
#endif

#include "utils/helpers.h"

#define HOST_STDIN_SEQUENCE_MAX 32
#define HOST_STDIN_ESCAPE_TIMEOUT US_TO_TSTATES(30000)

typedef enum {
    HOST_STDIN_TERMINAL_NORMAL,
    HOST_STDIN_TERMINAL_ESCAPE,
    HOST_STDIN_TERMINAL_CSI,
    HOST_STDIN_TERMINAL_SS3,
    HOST_STDIN_TERMINAL_REPLAY,
} host_stdin_terminal_state_t;

typedef struct {
    uint8_t pending_byte;
    int flags;
    bool enabled;
    bool flags_saved;
    bool eof;
    bool pending;
    bool previous_was_cr;
    host_stdin_terminal_state_t terminal_state;
    uint8_t sequence[HOST_STDIN_SEQUENCE_MAX];
    uint8_t sequence_len;
    uint8_t replay_pos;
    uint32_t escape_elapsed;
    bool replay_escape;
#if !defined(PLATFORM_WEB) && !defined(_WIN32)
    struct termios termios;
    bool is_tty;
    bool termios_saved;
#endif
} host_stdin_t;

struct keyboard;

int host_stdin_init(host_stdin_t* host_stdin, bool enabled);
void host_stdin_reset(host_stdin_t* host_stdin);
void host_stdin_deinit(host_stdin_t* host_stdin);
void host_stdin_tick(host_stdin_t* host_stdin, struct keyboard* keyboard, int elapsed);
bool host_stdin_type_byte(host_stdin_t* host_stdin, struct keyboard* keyboard, uint8_t byte);
bool host_stdin_terminal_byte(host_stdin_t* host_stdin, struct keyboard* keyboard, uint8_t byte);
