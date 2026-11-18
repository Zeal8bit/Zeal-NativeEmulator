/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Floating tool windows (Code & Profiler, Hardware): show/hide, refresh
 * and geometry persistence. Views register with tools/tool_registry.h. */
#pragma once

namespace zui::tools {

void tool_window_show(const char* id, bool show);
bool tool_window_shown(const char* id);
/** Refresh the visible view of every open tool window. */
void tool_windows_refresh(bool paused, bool force);
/** Hide all tool windows, remembering which were open (leaving the debugger). */
void tool_windows_suspend();
/** Reopen the tool windows that were open (entering the debugger). */
void tool_windows_resume();
void tool_windows_destroy();
/** Reset saved geometry/visibility of every tool window. */
void tool_windows_reset();

} // namespace zui::tools
