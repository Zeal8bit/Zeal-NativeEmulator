/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* CPU and raster state for the debugger's CPU panel (bridge/cpu.c).
 * Plain C data only, so the C++ UI never needs hw/zeal.h. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Z80 interrupt state */
    uint8_t  im;             /* interrupt mode: 0, 1 or 2 */
    bool     iff1;
    bool     iff2;
    bool     halted;         /* executing HALT, waiting for an interrupt */
    bool     int_pending;    /* maskable interrupt requested, not yet accepted */
    bool     nmi_pending;

    /* Timing, counted from the last reset */
    uint64_t tstates;        /* CPU cycle counter */
    uint64_t elapsed_us;     /* emulated time at the CPU clock */
    uint64_t frame;          /* video frames started since reset */

    /* ZVB raster position */
    int      scanline;       /* 0..total_lines-1, visible lines first */
    int      hpos;           /* pixel clock within the line, 0..line_pixels-1 */
    int      visible_lines;  /* 480 */
    int      total_lines;    /* 524 */
    int      visible_pixels; /* 640 */
    int      line_pixels;    /* 800 */
    bool     hblank;
    bool     vblank;
} fz_cpu_state_t;

/** Exact state right now. False when no machine is bound. */
bool fz_cpu_state(fz_cpu_state_t* out);

/**
 * While running: the state captured by the live sampler at the same instant
 * as the registers returned by fz_live_sample(). False if no sample yet.
 */
bool fz_cpu_state_sampled(fz_cpu_state_t* out);

#ifdef __cplusplus
}
#endif
