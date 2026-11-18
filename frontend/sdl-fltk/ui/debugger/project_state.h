/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Debugger state that belongs to the program being developed, saved in the
 * project settings (./zeal.ini, see common/persist.h): breakpoints,
 * watchpoints and the loaded map files. Watches save themselves
 * (watch_model). */
#pragma once

namespace zui::debugger {

/** Register the save hook and restore the saved state once the machine is
 *  bound (call once, when the debugger UI is created). */
void project_state_init();
/** Remember the current state for the next save (the debugger UI is
 *  closing; the machine is unbound before the configuration is saved). */
void project_state_capture();

} // namespace zui::debugger
