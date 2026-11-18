/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* "Break on read/write" of watch rows: a watch whose address does not
 * depend on the CPU state can stop the machine when its bytes are read or
 * written. Each such row becomes one core watchpoint (watch_breaks_sync).
 *
 * Breakable:  C000, symbol, table+4      its bytes (u8 1, u16 2, str N)
 *             (ptr), *u8 and *u16 types   the pointer word itself
 * Not:        (HL), (DE), (IX+n), (SP)... the address follows a register */
#pragma once

#include <cstddef>
#include "common/machine.h"
#include "debugger/watch_model.h"

namespace zui::debugger {

/** "R", "W" or "RW". */
const char* watch_type_short(watchpoint_type_t type);
/** "Read", "Write" or "Access" (both). */
const char* watch_type_name(watchpoint_type_t type);
/** "$C000" or "$C000-$C00F". */
void watch_range_text(char* out, size_t size, hwaddr lo, hwaddr hi);

/** Bytes a break on this watch covers; false (with `why`) when the watch
 *  cannot break because its address follows a register. */
bool watch_break_target(const Watch& w, hwaddr* lo, hwaddr* hi, const char** why = nullptr);

/** True when the watch can never break (its address follows a register);
 *  otherwise a failing watch_break_target() is only unresolved for now. */
bool watch_break_permanent(const Watch& w);

/** Make the core watchpoints match the watch rows (cheap when unchanged).
 *  Rows are projected per byte, so overlapping rows never alter each other;
 *  a row whose expression does not resolve yet keeps its break, unarmed. */
void watch_breaks_sync();

/** Break on [lo, hi] for `type` (memory view): sets the break of the watch
 *  covering exactly that range, or adds one. With a notification. */
bool watch_break_range(hwaddr lo, hwaddr hi, watchpoint_type_t type);
/** Clear the break of the watches covering `addr` (the rows stay). */
bool watch_break_clear_at(hwaddr addr);

/** Watch row whose break stopped the machine, -1 if none. */
int watch_hit_index();

/**
 * The watchpoint hit that stopped the machine, if it is still stopped
 * there: "Write $C000 = $3E at PC $1234" (`brief`: "Write $C000").
 */
bool watch_hit_text(char* out, size_t size, bool brief);

} // namespace zui::debugger
