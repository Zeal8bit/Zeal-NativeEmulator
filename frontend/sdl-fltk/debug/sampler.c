/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Live CPU sampling for the debugger while the machine runs.
 *
 * The UI only gets control between emulated frames, so reading registers
 * there always catches the CPU at the same point of the frame (typically a
 * wait-for-vblank loop) and looks frozen. Instead, a virtual timer copies the
 * CPU state at a random point inside each ~100 ms window of emulated time,
 * giving a representative picture of what the program is doing. */

#include <stdlib.h>
#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "utils/helpers.h"
#include "utils/vtimer.h"

/* Mean interval between samples, matching the UI refresh rate (10 Hz) */
#define SAMPLE_PERIOD_US    100000
#define SAMPLE_STACK_BYTES  64
#define SAMPLE_HL_BYTES     8

static vtimer_node_t s_timer;
static bool s_bound;
static struct {
    bool valid;
    regs_t regs;
    uint8_t stack[SAMPLE_STACK_BYTES];
    uint8_t hl[SAMPLE_HL_BYTES];
} s_sample;

static void schedule_next(void)
{
    /* Uniform in [0.5, 1.5) periods: never phase-locked to the 16.7 ms frame */
    const unsigned long base = US_TO_TSTATES(SAMPLE_PERIOD_US / 2);
    const unsigned long jitter = (unsigned long) rand() % US_TO_TSTATES(SAMPLE_PERIOD_US);
    vtimer_schedule_tstates(&s_timer, base + jitter);
}

static void sample_cb(void* userdata)
{
    zeal_t* machine = (zeal_t*) userdata;
    schedule_next();
    dbg_t* dbg = &machine->dbg;
    debugger_get_registers(dbg, &s_sample.regs);
    debugger_read_memory(dbg, s_sample.regs.sp, SAMPLE_STACK_BYTES, s_sample.stack);
    debugger_read_memory(dbg, s_sample.regs.hl, SAMPLE_HL_BYTES, s_sample.hl);
    fltk_cpu_sample(machine);
    s_sample.valid = true;
}

void fltk_sampler_bind(zeal_t* machine)
{
    memset(&s_sample, 0, sizeof(s_sample));
    vtimer_init_node(&s_timer, &machine->cpu.clock, sample_cb, machine);
    schedule_next();
    s_bound = true;
}

void fltk_sampler_unbind(void)
{
    if (!s_bound) {
        return;
    }
    vtimer_cancel(&s_timer);
    s_bound = false;
    s_sample.valid = false;
    fltk_cpu_sample_reset();
}

bool fz_live_sample(regs_t* regs, uint8_t* stack, int stack_len, uint8_t* hl, int hl_len)
{
    if (!s_sample.valid) {
        return false;
    }
    if (regs) {
        *regs = s_sample.regs;
    }
    if (stack && stack_len > 0) {
        const int n = stack_len < SAMPLE_STACK_BYTES ? stack_len : SAMPLE_STACK_BYTES;
        memcpy(stack, s_sample.stack, (size_t) n);
        if (stack_len > n) {
            memset(stack + n, 0, (size_t) (stack_len - n));
        }
    }
    if (hl && hl_len > 0) {
        const int n = hl_len < SAMPLE_HL_BYTES ? hl_len : SAMPLE_HL_BYTES;
        memcpy(hl, s_sample.hl, (size_t) n);
    }
    return true;
}
