/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Machine-side implementation of bridge/cpu.h: interrupt state, cycle
 * counter and ZVB raster position for the debugger's CPU panel. */

#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/bridge/cpu.h"
#include "utils/helpers.h"

/* ZVB raster timing (mirrors the private constants of hw/zvb/zvb.c) */
#define RASTER_PIXEL_NS        40ULL
#define RASTER_LINE_PIXELS     800
#define RASTER_TOTAL_LINES     524
#define RASTER_LINE_NS         (RASTER_LINE_PIXELS * RASTER_PIXEL_NS)
#define RASTER_FRAME_NS        (RASTER_TOTAL_LINES * RASTER_LINE_NS)
#define TSTATE_NS              (1000000000ULL / CPUFREQ)

static bool s_sample_valid;
static fz_cpu_state_t s_sample;

static void capture(zeal_t* machine, fz_cpu_state_t* out)
{
    z80* cpu = &machine->cpu;
    const zvb_t* zvb = &machine->zvb;
    memset(out, 0, sizeof(*out));

    out->im = cpu->interrupt_mode;
    out->iff1 = cpu->iff1;
    out->iff2 = cpu->iff2;
    out->halted = cpu->halted;
    out->int_pending = cpu->int_pending;
    out->nmi_pending = cpu->nmi_pending;

    out->tstates = (uint64_t) cpu->cyc;
    out->elapsed_us = TSTATES_TO_US((uint64_t) cpu->cyc);

    out->visible_lines = ZVB_MAX_RES_HEIGHT;
    out->total_lines = RASTER_TOTAL_LINES;
    out->visible_pixels = ZVB_MAX_RES_WIDTH;
    out->line_pixels = RASTER_LINE_PIXELS;
    out->scanline = zvb->current_scanline;
    out->hblank = zvb->status.h_blank;
    out->vblank = zvb->status.v_blank;

    /* The raster only stores the start of the current line; derive the exact
     * pixel position from the virtual clock. */
    const uint64_t now = vtimer_now_ns(&cpu->clock);
    uint64_t hpos = now > zvb->line_start_ns ? (now - zvb->line_start_ns) / RASTER_PIXEL_NS : 0;
    if (hpos >= RASTER_LINE_PIXELS) hpos = RASTER_LINE_PIXELS - 1;
    out->hpos = (int) hpos;

    /* Frames since reset: CPU cycles and the raster both restart at reset,
     * so the reset instant is `now - cyc`. Round to absorb sub-line skew. */
    const uint64_t since_reset = (uint64_t) cpu->cyc * TSTATE_NS;
    const uint64_t reset_ns = now > since_reset ? now - since_reset : 0;
    const uint64_t frame_start = zvb->line_start_ns - (uint64_t) zvb->current_scanline * RASTER_LINE_NS;
    out->frame = frame_start > reset_ns ? (frame_start - reset_ns + RASTER_FRAME_NS / 2) / RASTER_FRAME_NS : 0;
}

bool fz_cpu_state(fz_cpu_state_t* out)
{
    if (fltk_machine == NULL || out == NULL) return false;
    capture(fltk_machine, out);
    return true;
}

bool fz_cpu_state_sampled(fz_cpu_state_t* out)
{
    if (!s_sample_valid || out == NULL) return false;
    *out = s_sample;
    return true;
}

void fltk_cpu_sample(zeal_t* machine)
{
    capture(machine, &s_sample);
    s_sample_valid = true;
}

void fltk_cpu_sample_reset(void)
{
    s_sample_valid = false;
}
