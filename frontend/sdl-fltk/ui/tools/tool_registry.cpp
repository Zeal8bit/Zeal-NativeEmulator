/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tools/tool_registry.h"
#include <map>
#include <string>
#include <vector>

namespace zui::tools {

std::map<std::string, std::vector<ViewInfo>>& registered_views()
{
    static std::map<std::string, std::vector<ViewInfo>> views;
    return views;
}

ToolViewRegistrar::ToolViewRegistrar(const char* window, int order, const char* name, ToolViewFactory factory)
{
    registered_views()[window].push_back({ order, name, std::move(factory) });
}

} // namespace zui::tools
