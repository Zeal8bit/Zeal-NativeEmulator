/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/project_state.h"
#include <algorithm>
#include <set>
#include <FL/Fl.H>
#include "common/machine.h"
#include "common/persist.h"
#include "common/toml_config.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "debugger/watch_breaks.h"
#include "debugger/watch_model.h"
#include "frontend/sdl-fltk/bridge/symbols.h"

namespace zui::debugger {
using namespace zui::common;
namespace fs = std::filesystem;

namespace {

bool restored;
bool breakpoint_overflow;
std::vector<breakpoint_t> stops;
std::vector<fs::path> maps;
toml::array last_maps, last_stops, last_watches;

const toml::array& list(const char* key)
{
    static const toml::array empty;
    auto node = project_config()["debugger"][key];
    if (!node) return empty;
    if (auto* array = node.as_array()) return *array;
    throw std::runtime_error(std::string("debugger.") + key + " must be an array");
}

void check_fields(const toml::node& row, const char* list_name, std::initializer_list<std::string_view> allowed)
{
    auto* table = row.as_table();
    if (!table) throw std::runtime_error(std::string("expected a table in debugger.") + list_name);
    for (const auto& [key, value] : *table) {
        if (std::find(allowed.begin(), allowed.end(), key.str()) == allowed.end())
            throw std::runtime_error(std::string("unknown field in debugger.") + list_name + ": " + std::string(key.str()));
    }
}

template<typename T>
T field(const toml::node& row, const char* key, T fallback)
{
    auto node = toml::node_view{row}[key];
    if (!node) return fallback;
    if (auto value = node.value_exact<T>()) return *value;
    throw std::runtime_error(std::string("invalid debugger list field: ") + key);
}

toml::array encode_watches()
{
    toml::array out;
    for (const Watch& w : watches()) {
        toml::table row{{"expression", w.expr}, {"type", WATCH_TYPES[w.type].key}};
        if (w.type == W_STR) row.insert("length", w.len);
        if (w.brk) {
            row.insert("break_on", w.brk == WATCHPOINT_READ ? "read" : w.brk == WATCHPOINT_WRITE ? "write" : "read_write");
            if (!w.brk_on) row.insert("break_enabled", false);
        }
        row.is_inline(true);
        out.push_back(std::move(row));
    }
    return out;
}

void append_stop(toml::array& out, hwaddr addr, bool enabled)
{
    toml::table row;
    row.insert("address", static_cast<int64_t>(addr), toml::value_flags::format_as_hexadecimal);
    if (!enabled) row.insert("enabled", false);
    row.is_inline(true);
    out.push_back(std::move(row));
}

void append_map(toml::array& out, const fs::path& path)
{
    auto relative = fs::absolute(path).lexically_normal().lexically_relative(project_directory());
    out.push_back((relative.empty() ? path : relative).generic_string());
}

// Compare the decoded state. An untouched project keeps its spelling and comments.
void update_list(const char* key, toml::array values, toml::array& previous)
{
    if (values == previous) return;
    set_project_list(key, values);
    previous = std::move(values);
}

void capture()
{
    if (!restored || !dbg_ready()) return;
    if (!breakpoint_overflow) {
        toml::array values;
        hwaddr addresses[DBG_MAX_POINTS];
        dbg_t* dbg = fz_debugger();
        const int count = debugger_get_breakpoints(dbg, addresses, DBG_MAX_POINTS);
        for (int i = 0; i < count; i++)
            append_stop(values, addresses[i], debugger_is_breakpoint_enabled(dbg, addresses[i]));
        update_list("breakpoints", std::move(values), last_stops);
    }
    toml::array files;
    for (int i = 0; i < fz_map_count(); ++i) append_map(files, fz_map_path(i));
    update_list("maps", std::move(files), last_maps);
    update_list("watches", encode_watches(), last_watches);
}

void try_restore(void*)
{
    if (!dbg_ready()) Fl::repeat_timeout(0.05, try_restore);
    else project_state_restore();
}
} // namespace

void project_state_load()
{
    restored = breakpoint_overflow = false;
    stops.clear();
    maps.clear();
    watches().clear();
    last_maps.clear();
    last_stops.clear();
    for (const auto& entry : list("maps")) {
        auto path = entry.value<std::string>();
        if (!path || path->empty() || path->find('\0') != std::string::npos)
            throw std::runtime_error("debugger.maps requires nonempty path strings");
        maps.push_back((project_directory() / *path).lexically_normal());
        append_map(last_maps, maps.back());
    }
    std::set<hwaddr> addresses;
    if (list("breakpoints").size() > DBG_MAX_POINTS)
        throw std::runtime_error("debugger.breakpoints exceeds the machine limit of " + std::to_string(DBG_MAX_POINTS));
    for (const auto& entry : list("breakpoints")) {
        check_fields(entry, "breakpoints", {"address", "enabled"});
        auto addr = field<int64_t>(entry, "address", -1);
        if (addr < 0 || addr > 0xffff || !addresses.insert((hwaddr) addr).second)
            throw std::runtime_error("invalid or duplicate debugger breakpoint address");
        breakpoint_t bp{};
        bp.addr = (hwaddr) addr;
        bp.disabled = !field(entry, "enabled", true);
        stops.push_back(bp);
        append_stop(last_stops, bp.addr, !bp.disabled);
    }
    if (list("watches").size() > MAX_WATCHES)
        throw std::runtime_error("debugger.watches exceeds the watch limit of " + std::to_string(MAX_WATCHES));
    for (const auto& entry : list("watches")) {
        check_fields(entry, "watches", {"expression", "type", "length", "break_on", "break_enabled"});
        Watch w;
        w.expr = field(entry, "expression", std::string{});
        if (w.expr.empty() || w.expr.size() > MAX_EXPR || w.expr.find('\0') != std::string::npos)
            throw std::runtime_error("invalid watch expression");
        auto type = field(entry, "type", std::string("u8"));
        for (w.type = 0; w.type < W_COUNT && type != WATCH_TYPES[w.type].key; ++w.type) {}
        if (w.type == W_COUNT) throw std::runtime_error("unknown watch type: " + type);
        auto length = field<int64_t>(entry, "length", DEFAULT_STR);
        if (length < 1 || length > MAX_STR) throw std::runtime_error("watch length out of range");
        w.len = static_cast<int>(length);
        auto access = field(entry, "break_on", std::string{});
        if (access == "read") w.brk = WATCHPOINT_READ;
        else if (access == "write") w.brk = WATCHPOINT_WRITE;
        else if (access == "read_write") w.brk = WATCHPOINT_READ | WATCHPOINT_WRITE;
        else if (!access.empty()) throw std::runtime_error("unknown watch break_on: " + access);
        w.brk_on = field(entry, "break_enabled", true);
        watches().push_back(w);
    }
    last_watches = encode_watches();
}

void project_state_restore()
{
    if (restored || !dbg_ready()) return;
    Fl::remove_timeout(try_restore);
    int missing = 0;
    for (const auto& path : maps) {
        if (!fz_map_restore(path.string().c_str())) ++missing;
    }
    if (missing) notif_show("%d map file(s) not loaded (use Reload)", missing);
    auto* dbg = fz_debugger();
    for (const auto& bp : stops) {
        debugger_set_breakpoint(dbg, bp.addr);
        if (!debugger_has_breakpoint(dbg, bp.addr)) breakpoint_overflow = true;
        else debugger_enable_breakpoint(dbg, bp.addr, !bp.disabled);
    }
    if (breakpoint_overflow) notif_show("Breakpoint table full; saved breakpoint list preserved");
    watch_breaks_sync();
    restored = true;
    request_refresh();
}

void project_state_init()
{
    persist_register(capture);
    Fl::add_timeout(0.0, try_restore);
}

void project_state_close()
{
    Fl::remove_timeout(try_restore);
    restored = false;
}

} // namespace zui::debugger
