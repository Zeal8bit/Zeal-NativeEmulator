/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Cross-panel navigation and refresh requests. */
#pragma once

#include "common/machine.h"

namespace zui::debugger {

/** Refresh every debugger panel and tool view now (after a user action). */
void request_refresh();
/** Resume only after a stop at addr is armed. */
void run_to_address(hwaddr addr);
/** Show an address in the main window's memory editor / disassembly. */
void goto_memory(hwaddr addr);
void goto_disassembly(hwaddr addr);

} // namespace zui::debugger
