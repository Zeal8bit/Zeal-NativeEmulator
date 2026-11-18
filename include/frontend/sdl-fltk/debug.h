/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Debugger hooks used by the always-built machine code. Implemented by
 * debug/debug.c when the debugger is compiled in, by debug_disabled.c
 * otherwise (Meson picks one), so the machine code has no #if gates. */
#pragma once

#include <stdbool.h>
#include "hw/zeal.h"

/** Attach the debugger helpers (CPU sampler, PC profiler, UART console). */
void fltk_debug_bind(zeal_t* machine);
void fltk_debug_unbind(zeal_t* machine);
/** Debugger UI shown at startup (-g): start in debugger mode, running. */
void fltk_debug_start(zeal_t* machine);

/** The debugger is active: stepping, breakpoints and panels are live. */
bool fltk_debug_active(const zeal_t* machine);
/** Stopped at a breakpoint, a step or by the user. */
bool fltk_debug_paused(const zeal_t* machine);
/** False once the debugger asked the emulator to quit. */
bool fltk_debug_keep_running(const zeal_t* machine);
/** Keys must not reach the machine (debugger paused or stepping). */
bool fltk_debug_keyboard_blocked(const zeal_t* machine);
/** Reset through the debugger; false when the debugger is not in use. */
bool fltk_debug_reset(zeal_t* machine, bool pause_after);
