/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Self-registering views of the floating tool windows.
 *
 * A view is a Panel subclass in its own file that registers itself:
 *
 *     static widgets::Panel* make_symbols(int x, int y, int w, int h) { return new SymbolsView(x, y, w, h); }
 *     static tools::ToolViewRegistrar reg_symbols(tools::TOOL_CODE, 10, "Symbols", make_symbols);
 *
 * Views are created when their window first opens, shown one at a time via
 * the tab bar (ordered by `order`), and refreshed like other panels (only
 * the visible view, ~4 Hz while running, immediately on pause/step). */
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "widgets/panel.h"

namespace zui::tools {

/* Tool window identifiers */
constexpr const char* TOOL_CODE = "code";          /* Code & Profiler: symbols, hot functions, semihost */
constexpr const char* TOOL_HARDWARE = "hardware";  /* Hardware: video/sound/IO registers, input, console */

using ToolViewFactory = std::function<widgets::Panel*(int x, int y, int w, int h)>;

struct ToolViewRegistrar {
    ToolViewRegistrar(const char* window, int order, const char* name, ToolViewFactory factory);
};

struct ViewInfo {
    int order;
    std::string name;
    ToolViewFactory factory;
};

/** Views registered so far, by window id. */
std::map<std::string, std::vector<ViewInfo>>& registered_views();

} // namespace zui::tools
