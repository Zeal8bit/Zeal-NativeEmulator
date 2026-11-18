/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Debugger state that belongs to the program being developed, saved in the
 * project settings (./zeal.ini, see common/persist.h): breakpoints and the
 * loaded map files. Watches, with their memory breaks, save themselves
 * (watch_model). */
#pragma once

namespace zui::debugger {

/** Register the save hook (call once, when the debugger UI is created).
 *  The saved state is restored by project_state_restore(). */
void project_state_init();
/** Restore the saved maps, breakpoints and watch breaks now, once: called
 *  when the machine is bound, before it executes anything, so saved stops
 *  are in place for the first instruction. No-op without a machine. */
void project_state_restore();
/** Remember the current state for the next save (the debugger UI is
 *  closing; the machine is unbound before the configuration is saved). */
void project_state_capture();

} // namespace zui::debugger
