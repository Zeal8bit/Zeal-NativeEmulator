/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Statistical PC profiler (bridge_profile.c), used by the Hot Functions view.
 *
 * While profiling is on, a virtual timer on the CPU clock records the program
 * counter at random points of emulated time (mean interval configurable) into
 * a 64K-entry histogram indexed by the 16-bit logical address. Samples are
 * only taken while the machine executes, never while paused. Plain C types
 * only: included by the C++ UI. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FZ_PROFILE_SLOTS                65536
#define FZ_PROFILE_DEFAULT_INTERVAL_US  500

/** Start (or resume) sampling; existing samples are kept. */
void     fz_profile_start(void);
void     fz_profile_stop(void);
/** Clear the histogram and the profiled time (sampling state unchanged). */
void     fz_profile_reset(void);
bool     fz_profile_running(void);
/** Total number of samples in the histogram. */
uint64_t fz_profile_total(void);
/** Emulated time covered by the profile, in microseconds. */
uint64_t fz_profile_elapsed_us(void);
/** Histogram of FZ_PROFILE_SLOTS counters indexed by PC; returns the total.
 *  `counts` is set to NULL when profiling is unavailable (no debugger). */
uint64_t fz_profile_histogram(const uint32_t** counts);
/** Mean interval between samples, in microseconds of emulated time. */
unsigned fz_profile_interval_us(void);
void     fz_profile_set_interval_us(unsigned us);

#ifdef __cplusplus
}
#endif
