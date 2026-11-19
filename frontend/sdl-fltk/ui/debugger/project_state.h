/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace zui::debugger {

// Decode and validate the project before opening the UI.
void project_state_load();
void project_state_init();
// Maps and stops must be installed before the first guest instruction.
void project_state_restore();
void project_state_close();

} // namespace zui::debugger
