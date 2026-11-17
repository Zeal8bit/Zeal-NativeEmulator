/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Floating tool windows hosting self-registering views.
 *
 * A view is a Panel subclass in its own file that registers itself:
 *
 *     static Panel* make_symbols(int x, int y, int w, int h) { return new SymbolsView(x, y, w, h); }
 *     static ToolViewRegistrar reg_symbols(TOOL_CODE, 10, "Symbols", make_symbols);
 *
 * Views are created when their window first opens, shown one at a time via
 * the segmented bar (ordered by `order`), and refreshed like other panels
 * (only the visible view, ~4 Hz while running, immediately on pause/step).
 * Window geometry, visibility and the selected view are saved per window. */
#pragma once

#include <functional>
#include "panel.h"

namespace zui {

/* Tool window identifiers */
constexpr const char* TOOL_CODE = "code";          /* Code & Profiler: symbols, hot functions, semihost */
constexpr const char* TOOL_HARDWARE = "hardware";  /* Hardware: video/sound/IO registers, input, console */

using ToolViewFactory = std::function<Panel*(int x, int y, int w, int h)>;

struct ToolViewRegistrar {
    ToolViewRegistrar(const char* window, int order, const char* name, ToolViewFactory factory);
};

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

} // namespace zui
