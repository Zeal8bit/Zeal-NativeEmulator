/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Execution trace and heuristic call stack for the Trace and Call Stack
 * views (debug/trace.c). The core records them while the debugger is on;
 * see debugger/zeal_trace.h for the entry and frame layouts. Plain C types
 * only: included by the C++ UI. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "debugger/debugger_types.h"
#include "debugger/zeal_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Entries the ring holds (0 when the build has -Dtrace_depth=0). */
unsigned fz_trace_capacity(void);
/** Copy up to `max` entries, most recent first; returns the number copied. */
unsigned fz_trace_snapshot(zeal_trace_entry_t* out, unsigned max);
/** Changes whenever an entry is recorded or the trace is cleared. */
uint64_t fz_trace_serial(void);
void     fz_trace_clear(void);
bool     fz_trace_enabled(void);
void     fz_trace_set_enabled(bool enabled);
/** Disassemble a recorded instruction from its bytes (label = exact symbol at pc). */
int      fz_trace_disassemble(const zeal_trace_entry_t* entry, dbg_instr_t* instr);

/** Call stack, outermost frame first; returns the number of frames copied. */
int      fz_callstack(zeal_frame_t* out, int max);
/** Outer frames are missing (overflow, or the trace resumed inside calls). */
bool     fz_callstack_truncated(void);

#ifdef __cplusplus
}
#endif
