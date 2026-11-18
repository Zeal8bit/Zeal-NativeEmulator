/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Memory watchpoints (break on read/write) as the panels use them. */
#pragma once

#include <cstddef>
#include "common/machine.h"

namespace zui::debugger {

/** "R", "W" or "RW". */
const char* watch_type_short(watchpoint_type_t type);
/** "Read", "Write" or "Access" (both). */
const char* watch_type_name(watchpoint_type_t type);
/** "$C000" or "$C000-$C00F". */
void watch_range_text(char* out, size_t size, hwaddr lo, hwaddr hi);

/** Watch [lo, hi] for `type` (replacing the type and range of a watchpoint
 *  starting at `lo`), with a notification. */
bool watch_add(hwaddr lo, hwaddr hi, watchpoint_type_t type);
/** Remove the watchpoint starting at `addr`, with a notification. */
bool watch_remove(hwaddr addr);

/**
 * The watchpoint hit that stopped the machine, if it is still stopped
 * there: "Write $C000 = $3E at PC $1234" (`brief`: "Write $C000").
 */
bool watch_hit_text(char* out, size_t size, bool brief);

} // namespace zui::debugger
