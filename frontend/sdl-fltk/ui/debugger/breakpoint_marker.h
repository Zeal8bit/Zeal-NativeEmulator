/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The breakpoint marker shared by the breakpoint list and the disassembly. */
#pragma once

namespace zui::debugger {

/** Filled red dot = enabled breakpoint, red ring = disabled. */
void draw_breakpoint_marker(int x, int y, int size, bool enabled);

} // namespace zui::debugger
