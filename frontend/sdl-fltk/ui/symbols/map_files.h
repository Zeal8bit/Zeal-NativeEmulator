/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Loading map files into the debugger's symbol table from the UI. */
#pragma once

namespace zui::symbols {

/** Ask for a map file and load it: replaces the symbols, or adds to them
 *  when `append`. Returns true if a file was loaded. */
bool map_open_dialog(bool append);
/** Reload the loaded map files (after rebuilding the program). */
void map_reload();
/** Remove every symbol. */
void map_clear();

} // namespace zui::symbols
