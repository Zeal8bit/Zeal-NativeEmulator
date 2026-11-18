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
    fltk_console_bind();
}

void fltk_debug_unbind(zeal_t* machine)
{
    (void) machine;
    fltk_sampler_unbind();
    fltk_pcprof_unbind();
    fltk_debug_video_free();
}

void fltk_debug_start(zeal_t* machine)
{
    if (!machine->dbg_enabled) return;
    ui_debug_mode(true);
    /* Start running; the user pauses explicitly or with breakpoints. */
    machine->dbg_state = ST_RUNNING;
}

bool fltk_debug_active(const zeal_t* machine)
{
    return machine->dbg_enabled;
}

bool fltk_debug_paused(const zeal_t* machine)
{
    return machine->dbg_enabled && machine->dbg_state == ST_PAUSED;
}

bool fltk_debug_keep_running(const zeal_t* machine)
{
    return machine->dbg.running;
}

bool fltk_debug_keyboard_blocked(const zeal_t* machine)
{
    return machine->dbg_enabled && machine->dbg_state != ST_RUNNING;
}

bool fltk_debug_reset(zeal_t* machine, bool pause_after)
{
    dbg_t* dbg = &machine->dbg;
    if (dbg->reset_cb == NULL) return false;
    /* The core always halts after a reset with the debugger on */
    const bool was_running = machine->dbg_enabled && !fltk_debug_paused(machine);
    dbg->reset_cb(dbg);
    if (was_running && !pause_after) debugger_continue(dbg);
    return true;
}

/* ------------------------------------------------------------------ */
/*  bridge/machine.h: debugger state for the UI                        */
/* ------------------------------------------------------------------ */

dbg_t* fz_debugger(void)
{
    return fltk_machine ? &fltk_machine->dbg : NULL;
}

bool fz_debugger_enabled(void)
{
    return fltk_machine && fltk_machine->dbg_enabled;
}

bool fz_paused(void)
{
    return fltk_machine && fltk_debug_paused(fltk_machine);
}

void fz_debugger_set_enabled(bool enabled)
{
    zeal_t* machine = fltk_machine;
    if (machine == NULL || machine->dbg_enabled == enabled) return;
    config_window_update(machine->dbg_enabled);
    machine->dbg_enabled = enabled;
    /* Showing or hiding the debugger never stops the machine; pause from
     * the toolbar (or a breakpoint) when needed */
    machine->dbg_state = ST_RUNNING;
    log_printf("[DEBUGGER]: %s\n", enabled ? "Enabled" : "Disabled");
    fz_keys_release_all();
    ui_debug_mode(enabled);
    ui_debug_refresh(true);
}

bool fz_vram_render(dbg_vram_t view, fz_image_t* out)
{
    if (fltk_machine == NULL) return false;
    return fltk_debug_video_render(&fltk_machine->zvb, view, out);
}

bool fz_sprite(int index, fz_sprite_t* info, uint8_t* rgb, uint8_t* mask)
{
    if (fltk_machine == NULL) return false;
    return fltk_debug_sprite(&fltk_machine->zvb, index, info, rgb, mask);
}
