/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tools/tool_windows.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include "common/machine.h"
#include "common/persist.h"
#include "tools/tool_registry.h"
#include "tools/tool_window.h"
#include "memtools/pattern.h"

namespace zui::tools {

using namespace zui::memtools;
using namespace zui::common;

static const WindowDef s_defs[] = {
    { TOOL_CODE, FZ_WIN_NAME " - Code & Profiler", "code_window" },
    { TOOL_HARDWARE, FZ_WIN_NAME " - Hardware", "hardware_window" },
};

static std::map<std::string, ToolWindow*>& windows()
{
    static std::map<std::string, ToolWindow*> map;
    return map;
}

static bool s_persist_registered;

static const WindowDef* find_def(const char* id)
{
    for (const WindowDef& d : s_defs) {
        if (strcmp(d.id, id) == 0) return &d;
    }
    return nullptr;
}

static ToolWindow* get(const char* id, bool create)
{
    auto it = windows().find(id);
    if (it != windows().end()) return it->second;
    if (!create) return nullptr;
    const WindowDef* def = find_def(id);
    if (def == nullptr) return nullptr;
    ToolWindow* w = new ToolWindow(*def);
    windows()[id] = w;
    return w;
}

static void persist_once()
{
    if (s_persist_registered) return;
    s_persist_registered = true;
    persist_register([]() {
        for (const WindowDef& d : s_defs) {
            ToolWindow* w = get(d.id, false);
            if (w) w->save();
        }
    });
}

void tool_window_show(const char* id, bool show)
{
    persist_once();
    ToolWindow* w = get(id, show);
    if (w == nullptr) return;
    if (show) w->open(); else w->close();
}

bool tool_window_shown(const char* id)
{
    ToolWindow* w = get(id, false);
    return w && w->shown();
}

void tool_windows_refresh(bool paused, bool force)
{
    for (auto& entry : windows()) entry.second->refresh(paused, force);
}

void tool_windows_suspend()
{
    for (auto& entry : windows()) entry.second->suspend();
}

void tool_windows_resume()
{
    persist_once();
    for (const WindowDef& d : s_defs) {
        ToolWindow* w = get(d.id, false);
        const bool wanted = w ? w->wanted() : preference(d.key, "shown", false);
        if (wanted) tool_window_show(d.id, true);
    }
}

void tool_windows_destroy()
{
    /* Final teardown (the debugger UI is deleted): their settings were
     * already taken by persist_finalize(), so the windows go too */
    for (auto& entry : windows()) {
        entry.second->hide();
        delete entry.second;
    }
    windows().clear();
    s_persist_registered = false;
}

void tool_windows_reset()
{
    for (auto& entry : windows()) {
        entry.second->hide();
        entry.second->reset();
    }
}

} // namespace zui::tools
