/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "hw/zeal.h"

/* Command handler signature used by the console dispatch table. argv[0] is
 * always the command name. */
typedef void (*console_cmd_fn)(zeal_t* machine, int argc, char** argv);

typedef struct {
    const char* name;
    console_cmd_fn handler;
} console_cmd_t;

/**
 * @brief Run the interactive console, reading one command per line from stdin.
 *
 * Commands:
 *   help                  Show available commands
 *   press <key>           Press and hold a key
 *   release <key>         Release a key
 *   tap <key>             Press and release a key
 *   run <tstates>         Advance the emulation by N Z80 T-states
 *   reset                 Reset the emulator
 *   quit                  Exit the emulator
 *
 * When started with --debug, the debugger commands are also available:
 * c/continue, s/step, so/step_over, p/pause, bp, bc, bl, regs, rb, r16, r32,
 * wb, u and mmu (type `help` for the full list).
 *
 * Keyboard events are injected into the existing keyboard device, so they are
 * processed exactly like physical keys. The 'run' command is based on emulated
 * Z80 T-states, which makes test scripts deterministic and faster than real time.
 *
 * Returns when stdin reaches EOF or the 'quit' command is received.
 */
void console_run(zeal_t* machine);
