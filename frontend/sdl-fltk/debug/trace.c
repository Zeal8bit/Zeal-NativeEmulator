/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Execution trace and call stack for the UI (bridge/trace.h): thin wrappers
 * over the core's recorder (hw/zeal_trace.c) for the bound machine. */

#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/bridge/trace.h"
#include "debugger/zeal_debugger.h"

unsigned fz_trace_capacity(void)
{
    return zeal_trace_capacity();
}

unsigned fz_trace_snapshot(zeal_trace_entry_t* out, unsigned max)
{
    if (fltk_machine == NULL || out == NULL) return 0;
    unsigned n = 0;
    while (n < max && zeal_trace_get(fltk_machine, n, &out[n])) n++;
    return n;
}

uint64_t fz_trace_serial(void)
{
    return zeal_trace_serial(fltk_machine);
}

void fz_trace_clear(void)
{
    zeal_trace_clear(fltk_machine);
}

bool fz_trace_enabled(void)
{
    return zeal_trace_enabled(fltk_machine);
}

void fz_trace_set_enabled(bool enabled)
{
    zeal_trace_set_enabled(fltk_machine, enabled);
}

int fz_trace_disassemble(const zeal_trace_entry_t* entry, dbg_instr_t* instr)
{
    if (fltk_machine == NULL || entry == NULL || instr == NULL) return -1;
    memset(instr, 0, sizeof(*instr));
    dbg_t* dbg = &fltk_machine->dbg;
    const int size = zeal_debugger_disassemble_bytes(dbg, (uint16_t) (entry->pc + entry->skipped_prefixes), entry->bytes, instr);
    const char* label = debugger_get_symbol(dbg, entry->pc);
    if (label) {
        strncpy(instr->label, label, sizeof(instr->label) - 1);
    }
    return size;
}

int fz_callstack(zeal_frame_t* out, int max)
{
    return zeal_callstack_get(fltk_machine, out, max);
}

bool fz_callstack_truncated(void)
{
    return zeal_callstack_truncated(fltk_machine);
}
