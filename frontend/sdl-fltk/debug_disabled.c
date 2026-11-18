/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Debugger hooks (debug.h) when the debugger is compiled out. */

#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/debug.h"

void fltk_debug_bind(zeal_t* machine)
{
    (void) machine;
}

void fltk_debug_unbind(zeal_t* machine)
{
    (void) machine;
}

void fltk_debug_start(zeal_t* machine)
{
    (void) machine;
}

bool fltk_debug_active(const zeal_t* machine)
{
    (void) machine;
    return false;
}

bool fltk_debug_paused(const zeal_t* machine)
{
    (void) machine;
    return false;
}

bool fltk_debug_keep_running(const zeal_t* machine)
{
    (void) machine;
    return true;
}

bool fltk_debug_keyboard_blocked(const zeal_t* machine)
{
    (void) machine;
    return false;
}

bool fltk_debug_reset(zeal_t* machine, bool pause_after)
{
    (void) machine;
    (void) pause_after;
    return false;
}

dbg_t* fz_debugger(void)
{
    return NULL;
}

bool fz_debugger_enabled(void)
{
    return false;
}

bool fz_paused(void)
{
    return false;
}

void fz_debugger_set_enabled(bool enabled)
{
    (void) enabled;
}
