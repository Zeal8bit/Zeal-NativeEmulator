/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Machine-side implementation of bridge.h for the C++ FLTK user interface. */

#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "utils/config.h"
#include "utils/helpers.h"
#include "utils/log.h"
#include "utils/notif.h"

static bool s_muted;

/* ------------------------------------------------------------------ */
/*  Machine control                                                    */
/* ------------------------------------------------------------------ */

void fz_reset(void)
{
    if (fltk_machine == NULL) return;
#if CONFIG_ENABLE_DEBUGGER
    dbg_t* dbg = &fltk_machine->dbg;
    if (dbg->reset_cb) {
        dbg->reset_cb(dbg);
        return;
    }
#endif
    zeal_reset(fltk_machine);
}

void fz_quit(void)
{
    if (fltk_machine == NULL) return;
    fltk_machine->should_exit = true;
}

dbg_t* fz_debugger(void)
{
#if CONFIG_ENABLE_DEBUGGER
    return fltk_machine ? &fltk_machine->dbg : NULL;
#else
    return NULL;
#endif
}

bool fz_debugger_enabled(void)
{
#if CONFIG_ENABLE_DEBUGGER
    return fltk_machine && fltk_machine->dbg_enabled;
#else
    return false;
#endif
}

bool fz_paused(void)
{
#if CONFIG_ENABLE_DEBUGGER
    return fltk_machine && fltk_machine->dbg_enabled && fltk_machine->dbg_state == ST_PAUSED;
#else
    return false;
#endif
}

/* ------------------------------------------------------------------ */
/*  Audio                                                              */
/* ------------------------------------------------------------------ */

static void apply_gain(void)
{
    fltk_audio_set_gain(s_muted ? 0.0f : config.audio.volume / 100.0f);
}

int fz_volume(void)
{
    return config.audio.volume;
}

void fz_volume_set(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    config.audio.volume = percent;
    apply_gain();
}

bool fz_muted(void)
{
    return s_muted;
}

void fz_muted_set(bool muted)
{
    s_muted = muted;
    apply_gain();
}

/* ------------------------------------------------------------------ */
/*  Video                                                              */
/* ------------------------------------------------------------------ */

bool fz_scanline_available(void)
{
#if ZVB_SCANLINE_RENDERING
    return true;
#else
    return false;
#endif
}

bool fz_scanline(void)
{
#if ZVB_SCANLINE_RENDERING
    return fltk_machine && zvb_scanline_rendering_requested(&fltk_machine->zvb);
#else
    return false;
#endif
}

void fz_scanline_set(bool enabled)
{
#if ZVB_SCANLINE_RENDERING
    if (fltk_machine) zvb_set_scanline_rendering(&fltk_machine->zvb, enabled);
#else
    (void) enabled;
#endif
}

fz_video_mode_t fz_video_mode(void)
{
    if (fltk_machine == NULL) return FZ_VIDEO_TEXT;
    const zvb_t* zvb = &fltk_machine->zvb;
    if (zvb_is_bitmap_mode(zvb)) return FZ_VIDEO_BITMAP;
    if (zvb_is_gfx_mode(zvb)) return FZ_VIDEO_GFX;
    return FZ_VIDEO_TEXT;
}

bool fz_vram_render(dbg_vram_t view, fz_image_t* out)
{
#if CONFIG_ENABLE_DEBUGGER
    if (fltk_machine == NULL) return false;
    return fltk_debug_video_render(&fltk_machine->zvb, view, out);
#else
    (void) view;
    (void) out;
    return false;
#endif
}

bool fz_gfx_screen(int* width, int* height)
{
    if (fltk_machine == NULL || !zvb_is_gfx_mode(&fltk_machine->zvb)) return false;
    const int mode = fltk_machine->zvb.mode;
    const bool mode_320 = mode == MODE_GFX_320_8BIT || mode == MODE_GFX_320_4BIT;
    if (width) *width = mode_320 ? 320 : 640;
    if (height) *height = mode_320 ? 240 : 480;
    return true;
}

bool fz_sprite(int index, fz_sprite_t* info, uint8_t* rgb, uint8_t* mask)
{
#if CONFIG_ENABLE_DEBUGGER
    if (fltk_machine == NULL) return false;
    return fltk_debug_sprite(&fltk_machine->zvb, index, info, rgb, mask);
#else
    (void) index;
    (void) info;
    (void) rgb;
    (void) mask;
    return false;
#endif
}

/* ------------------------------------------------------------------ */
/*  SNES adapter                                                       */
/* ------------------------------------------------------------------ */

int fz_snes_gamepad_count(void)
{
    return SNES_GAMEPAD_COUNT;
}

bool fz_snes_gamepad_available(int index)
{
    return index >= 0 && index < SNES_GAMEPAD_COUNT && snes_controller_available((uint8_t) index);
}

const char* fz_snes_gamepad_name(int index)
{
    if (index < 0 || index >= SNES_GAMEPAD_COUNT) return NULL;
    return snes_controller_name((uint8_t) index);
}

int fz_snes_gamepad_port(int index)
{
    if (index < 0 || index >= SNES_GAMEPAD_COUNT || frontend_snes.core == NULL) return SNES_PORT_DETACHED;
    return snes_adapter_get_controller_port(&frontend_snes, (uint8_t) index);
}

void fz_snes_gamepad_set_port(int index, int port)
{
    if (index < 0 || index >= SNES_GAMEPAD_COUNT || frontend_snes.core == NULL) return;
    snes_adapter_set_controller_port(&frontend_snes, (uint8_t) index, port);
}

int fz_snes_mouse_port(void)
{
    if (frontend_snes.core == NULL) return SNES_PORT_DETACHED;
    return snes_adapter_get_mouse_port(&frontend_snes);
}

void fz_snes_mouse_set_port(int port)
{
    if (frontend_snes.core == NULL) return;
    snes_adapter_set_mouse_port(&frontend_snes, port);
}

void fz_snes_mouse_reset_scale(void)
{
    if (frontend_snes.core == NULL) return;
    snes_adapter_reset_mouse_scale(&frontend_snes);
}

/* ------------------------------------------------------------------ */
/*  Semihost counters                                                  */
/* ------------------------------------------------------------------ */

int fz_semihost_counter_count(void)
{
    return SEMIHOST_MAX_COUNTERS;
}

bool fz_semihost_counter(int index, fz_counter_t* out)
{
    if (fltk_machine == NULL || out == NULL || index < 0 || index >= SEMIHOST_MAX_COUNTERS) return false;
    const semihost_t* semihost = &fltk_machine->semihost;
    const semihost_counter_t* counter = &semihost->counters[index];

    memset(out, 0, sizeof(*out));
    out->running = counter->running;
    out->break_on_update = counter->break_on_update;
    out->min_us = counter->min_us;
    out->max_us = counter->max_us;
    out->avg_us = counter->sample_count == 0 ? 0 : counter->total_interval_us / counter->sample_count;
    if (counter->running && semihost->cpu != NULL) {
        out->total_us = TSTATES_TO_US(semihost->cpu->cyc - counter->start_cyc);
    } else {
        out->total_us = counter->last_total_us;
    }
    return true;
}

void fz_semihost_toggle_break(int index)
{
    if (fltk_machine == NULL || index < 0 || index >= SEMIHOST_MAX_COUNTERS) return;
    semihost_counter_t* counter = &fltk_machine->semihost.counters[index];
    counter->break_on_update = !counter->break_on_update;
}
