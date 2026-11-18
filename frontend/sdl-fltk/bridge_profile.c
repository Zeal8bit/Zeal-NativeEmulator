/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Statistical PC profiler for the Hot Functions view.
 *
 * Same technique as sampler.c: a virtual timer on the CPU clock fires at a
 * random point of each sampling window (uniform in [0.5, 1.5) intervals), so
 * samples never phase-lock to the 16.7 ms frame or to periodic interrupts.
 * The timer is only armed while profiling is on: the profiler costs nothing
 * when idle and one counter increment per sample otherwise.
 *
 * The recorded PC is the next instruction to execute when the timer fires
 * (instruction boundary), which is accurate at function granularity. */

#include <stdlib.h>
#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/bridge_profile.h"
#include "utils/helpers.h"
#include "utils/vtimer.h"

#if CONFIG_ENABLE_DEBUGGER

#define MIN_INTERVAL_US     20
#define MAX_INTERVAL_US     100000

static zeal_t* s_machine;
static vtimer_node_t s_timer;
static bool s_running;
static unsigned s_interval_us = FZ_PROFILE_DEFAULT_INTERVAL_US;
static uint32_t s_counts[FZ_PROFILE_SLOTS];
static uint64_t s_total;
static uint64_t s_elapsed_ns;   /* profiled time accounted until s_mark_ns */
static uint64_t s_mark_ns;      /* clock time of the last accounting point */

static uint64_t clock_ns(void)
{
    return vtimer_now_ns(&s_machine->cpu.clock);
}

static void account(void)
{
    const uint64_t now = clock_ns();
    if (now > s_mark_ns) s_elapsed_ns += now - s_mark_ns;
    s_mark_ns = now;
}

static void schedule_next(void)
{
    const unsigned long period = US_TO_TSTATES((unsigned long) s_interval_us);
    const unsigned long jitter = (unsigned long) rand() % period;
    vtimer_schedule_tstates(&s_timer, period / 2 + jitter + 1);
}

static void sample_cb(void* userdata)
{
    zeal_t* machine = (zeal_t*) userdata;
    schedule_next();
    /* Single steps also advance the clock: only count free-running code */
    if (machine->dbg_enabled && machine->dbg_state != ST_RUNNING) {
        s_mark_ns = clock_ns();
        return;
    }
    account();
    uint32_t* slot = &s_counts[machine->cpu.pc];
    if (*slot != UINT32_MAX) {
        (*slot)++;
        s_total++;
    }
}

static void arm(void)
{
    s_mark_ns = clock_ns();
    schedule_next();
}

void fltk_pcprof_bind(zeal_t* machine)
{
    s_machine = machine;
    vtimer_init_node(&s_timer, &machine->cpu.clock, sample_cb, machine);
    if (s_running) arm();
}

void fltk_pcprof_unbind(void)
{
    if (s_machine == NULL) return;
    if (s_running) {
        account();
        vtimer_cancel(&s_timer);
    }
    s_machine = NULL;
}

void fz_profile_start(void)
{
    if (s_running) return;
    s_running = true;
    if (s_machine) arm();
}

void fz_profile_stop(void)
{
    if (!s_running) return;
    s_running = false;
    if (s_machine == NULL) return;
    account();
    vtimer_cancel(&s_timer);
}

void fz_profile_reset(void)
{
    memset(s_counts, 0, sizeof(s_counts));
    s_total = 0;
    s_elapsed_ns = 0;
    if (s_machine) s_mark_ns = clock_ns();
}

bool fz_profile_running(void)
{
    return s_running;
}

uint64_t fz_profile_total(void)
{
    return s_total;
}

uint64_t fz_profile_elapsed_us(void)
{
    uint64_t ns = s_elapsed_ns;
    if (s_running && s_machine && !(s_machine->dbg_enabled && s_machine->dbg_state != ST_RUNNING)) {
        const uint64_t now = clock_ns();
        if (now > s_mark_ns) ns += now - s_mark_ns;
    }
    return ns / 1000;
}

uint64_t fz_profile_histogram(const uint32_t** counts)
{
    if (counts) *counts = s_counts;
    return s_total;
}

unsigned fz_profile_interval_us(void)
{
    return s_interval_us;
}

void fz_profile_set_interval_us(unsigned us)
{
    if (us < MIN_INTERVAL_US) us = MIN_INTERVAL_US;
    if (us > MAX_INTERVAL_US) us = MAX_INTERVAL_US;
    s_interval_us = us;
}

#else

void fltk_pcprof_bind(zeal_t* machine)
{
    (void) machine;
}

void fltk_pcprof_unbind(void)
{
}

void fz_profile_start(void)
{
}

void fz_profile_stop(void)
{
}

void fz_profile_reset(void)
{
}

bool fz_profile_running(void)
{
    return false;
}

uint64_t fz_profile_total(void)
{
    return 0;
}

uint64_t fz_profile_elapsed_us(void)
{
    return 0;
}

uint64_t fz_profile_histogram(const uint32_t** counts)
{
    if (counts) *counts = NULL;
    return 0;
}

unsigned fz_profile_interval_us(void)
{
    return FZ_PROFILE_DEFAULT_INTERVAL_US;
}

void fz_profile_set_interval_us(unsigned us)
{
    (void) us;
}

#endif
