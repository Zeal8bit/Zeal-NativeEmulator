/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Debugger hooks (debug.h) and the debugger part of bridge/machine.h. */

#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/debug.h"
#include "utils/config.h"
#include "utils/log.h"

void fltk_debug_bind(zeal_t* machine)
{
    fltk_sampler_bind(machine);
    fltk_pcprof_bind(machine);
    fltk_console_bind(machine);
}

void fltk_debug_unbind(zeal_t* machine)
{
    (void) machine;
    fltk_sampler_unbind();
    fltk_pcprof_unbind();
    video_inspect_free();
}

void fltk_debug_start(zeal_t* machine)
{
    /* Start running; the user pauses explicitly or with breakpoints. */
    debugger_clear_stop_reason(&machine->dbg);
    machine->dbg_state = ST_RUNNING;
}

bool fltk_debug_paused(const zeal_t* machine)
{
    return machine->dbg_state == ST_PAUSED;
}

bool fltk_debug_keep_running(const zeal_t* machine)
{
    return machine->dbg.running;
}

bool fltk_debug_keyboard_blocked(const zeal_t* machine)
{
    return machine->dbg_state != ST_RUNNING;
}

bool fltk_debug_reset(zeal_t* machine, bool pause_after)
{
    dbg_t* dbg = &machine->dbg;
    if (dbg->reset_cb == NULL) {
        return false;
    }
    /* The core always halts after a reset with the debugger on */
    const bool was_running = !fltk_debug_paused(machine);
    dbg->reset_cb(dbg);
    if (was_running && !pause_after) {
        debugger_continue(dbg);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  bridge/machine.h: debugger state for the UI                        */
/* ------------------------------------------------------------------ */

dbg_t* fz_debugger(void)
{
    return fltk_machine ? &fltk_machine->dbg : NULL;
}

bool fz_paused(void)
{
    return fltk_machine && fltk_debug_paused(fltk_machine);
}

bool fz_vram_render(dbg_vram_t view, fz_image_t* out)
{
    if (fltk_machine == NULL) {
        return false;
    }
    return video_inspect_render(&fltk_machine->zvb, view, out);
}
