/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Machine state as the debugger panels show it. */
#pragma once

#include <cstdint>
#include "common/machine.h"

namespace zui::debugger {

/** True once the debugger is available (machine bound). */
bool   dbg_ready();
/** Registers to display: exact when paused, the mid-frame sample while
 *  running (reading them between frames always lands on the frame boundary). */
regs_t current_regs();
/** Registers for editing: always the exact current state. */
regs_t exact_regs();
/** Memory at SP (stack=true) or HL as displayed: sampled while running. */
void   display_memory(hwaddr addr, int len, uint8_t* out, bool stack);

} // namespace zui::debugger
