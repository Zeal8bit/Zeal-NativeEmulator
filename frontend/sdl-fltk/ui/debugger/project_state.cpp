/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/project_state.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl.H>
#include <FL/filename.H>
#include "common/machine.h"
#include "common/persist.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "debugger/watch_breaks.h"
#include "debugger/watch_model.h"
#include "frontend/sdl-fltk/bridge/symbols.h"

namespace zui::debugger {

using namespace zui::common;

namespace {

/* Saved as BREAKPOINT_<n> = "C000" or "C000 off" and MAP_FILE_<n> = path
 * (relative to the project directory when inside it). Memory breaks are
 * part of the watches (FLTK_WATCH_<n>, watch_model). */
constexpr int MAX_ITEMS = 128;

bool s_restored;
/* Taken when the debugger UI closes: the machine is unbound before the
 * configuration is saved */
bool s_captured;
std::vector<std::string> s_breakpoints, s_maps;

std::string key_of(const char* list, int index)
{
    char key[40];
    snprintf(key, sizeof(key), "%s_%d", list, index);
    return key;
}

std::vector<std::string> saved_list(const char* list)
{
    std::vector<std::string> out;
    const int count = config_get((std::string(list) + "_COUNT").c_str(), 0);
    for (int i = 0; i < count && i < MAX_ITEMS; i++) {
        const char* value = config_get_text(key_of(list, i).c_str(), nullptr);
        if (value && *value) out.push_back(value);
    }
    return out;
}

void write_list(rini_config* ini, const char* list, const std::vector<std::string>& items, const char* what)
{
    /* Nothing written for an empty list: no project file is created in a
     * directory that has nothing to save */
    if (items.empty()) return;
    rini_set_config_value(ini, (std::string(list) + "_COUNT").c_str(), (int) items.size(), what);
    for (size_t i = 0; i < items.size(); i++) {
        rini_set_config_value_text(ini, key_of(list, (int) i).c_str(), items[i].c_str(), "");
    }
}

/** Path to save: relative to the current (project) directory when inside it. */
std::string project_relative(const char* path)
{
    char cwd[FL_PATH_MAX];
    if (fl_getcwd(cwd, sizeof(cwd)) == nullptr) return path;
    std::string dir(cwd);
    if (!dir.empty() && dir.back() != '/') dir += '/';
    if (strncmp(path, dir.c_str(), dir.size()) == 0) return path + dir.size();
    return path;
}

std::vector<std::string> current_breakpoints()
{
    std::vector<std::string> out;
    dbg_t* dbg = fz_debugger();
    hwaddr bps[DBG_MAX_POINTS];
    const int count = debugger_get_breakpoints(dbg, bps, DBG_MAX_POINTS);
    for (int i = 0; i < count; i++) {
        char text[24];
        snprintf(text, sizeof(text), "%04X%s", (unsigned) bps[i],
                 debugger_is_breakpoint_enabled(dbg, bps[i]) ? "" : " off");
        out.push_back(text);
    }
    return out;
}

std::vector<std::string> current_maps()
{
    std::vector<std::string> out;
    for (int i = 0; i < fz_map_count(); i++) out.push_back(project_relative(fz_map_path(i)));
    return out;
}

void save(rini_config* ini)
{
    if (s_captured) {
        write_list(ini, "BREAKPOINT", s_breakpoints, "Breakpoints (hex address, \"off\" = disabled)");
        write_list(ini, "MAP_FILE", s_maps, "Map files");
        return;
    }
    if (!s_restored || !dbg_ready()) {
        /* The machine never came up: keep what was loaded */
        write_list(ini, "BREAKPOINT", saved_list("BREAKPOINT"), "Breakpoints");
        write_list(ini, "MAP_FILE", saved_list("MAP_FILE"), "Map files");
        return;
    }
    write_list(ini, "BREAKPOINT", current_breakpoints(), "Breakpoints (hex address, \"off\" = disabled)");
    write_list(ini, "MAP_FILE", current_maps(), "Map files");
}

void restore()
{
    dbg_t* dbg = fz_debugger();
    /* Maps first: watch breaks and other saved state may name their
     * symbols. Keep any -m map, add the project's others. */
    for (const std::string& path : saved_list("MAP_FILE")) {
        char full[FL_PATH_MAX];
        fl_filename_absolute(full, sizeof(full), path.c_str());
        bool loaded = false;
        for (int i = 0; i < fz_map_count(); i++) {
            char other[FL_PATH_MAX];
            fl_filename_absolute(other, sizeof(other), fz_map_path(i));
            loaded = loaded || strcmp(full, other) == 0;
        }
        if (!loaded) fz_map_open(full, true);
    }
    for (const std::string& item : saved_list("BREAKPOINT")) {
        const hwaddr addr = (hwaddr) strtoul(item.c_str(), nullptr, 16) & 0xFFFF;
        debugger_set_breakpoint(dbg, addr);
        if (item.find(" off") != std::string::npos) debugger_enable_breakpoint(dbg, addr, false);
    }
    /* Older builds saved watchpoints separately: they become watch rows
     * with a break (watch_breaks.h) */
    for (const std::string& item : saved_list("WATCHPOINT")) {
        char type[4] = "";
        unsigned start = 0, end = 0;
        const int n = sscanf(item.c_str(), "%3s %x-%x", type, &start, &end);
        if (n < 2 || (int) watches().size() >= MAX_WATCHES) continue;
        Watch w;
        char expr[8];
        snprintf(expr, sizeof(expr), "$%04X", start & 0xFFFF);
        w.expr = expr;
        const int bytes = n == 3 && end > start ? (int) (end - start) + 1 : 1;
        w.type = bytes == 1 ? W_U8 : bytes == 2 ? W_U16 : W_STR;
        if (w.type == W_STR) w.len = bytes > MAX_STR ? MAX_STR : bytes;
        if (strchr(type, 'r')) w.brk |= WATCHPOINT_READ;
        if (strchr(type, 'w')) w.brk |= WATCHPOINT_WRITE;
        w.brk_on = item.find(" off") == std::string::npos;
        if (w.brk) watches().push_back(w);
    }
    watch_breaks_sync();
    s_restored = true;
    request_refresh();
}

/* Fallback for a debugger UI created after the machine started: restore as
 * soon as a machine is bound */
void try_restore(void*)
{
    if (s_restored) return;
    if (!dbg_ready()) {
        Fl::repeat_timeout(0.05, try_restore);
        return;
    }
    restore();
}

} // namespace

void project_state_restore()
{
    if (s_restored || !dbg_ready()) return;
    Fl::remove_timeout(try_restore);
    restore();
}

void project_state_capture()
{
    if (!s_restored || !dbg_ready()) return;
    s_breakpoints = current_breakpoints();
    s_maps = current_maps();
    s_captured = true;
}

void project_state_init()
{
    persist_register_project(save);
    Fl::add_timeout(0.0, try_restore);
}

} // namespace zui::debugger
