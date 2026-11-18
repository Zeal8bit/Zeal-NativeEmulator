/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/watchpoint_actions.h"
#include <cstddef>
#include <cstdio>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"

namespace zui::debugger {

using namespace zui::common;

const char* watch_type_short(watchpoint_type_t type)
{
    switch (type & WATCHPOINT_RW) {
        case WATCHPOINT_READ: return "R";
        case WATCHPOINT_WRITE: return "W";
        default: return "RW";
    }
}

const char* watch_type_name(watchpoint_type_t type)
{
    switch (type & WATCHPOINT_RW) {
        case WATCHPOINT_READ: return "Read";
        case WATCHPOINT_WRITE: return "Write";
        default: return "Access";
    }
}

void watch_range_text(char* out, size_t size, hwaddr lo, hwaddr hi)
{
    char a[8];
    char b[8];
    hex16(a, sizeof(a), lo);
    if (hi > lo) {
        hex16(b, sizeof(b), hi);
        snprintf(out, size, "$%s-$%s", a, b);
    } else {
        snprintf(out, size, "$%s", a);
    }
}

bool watch_add(hwaddr lo, hwaddr hi, watchpoint_type_t type)
{
    if (!dbg_ready()) return false;
    watchpoint_t wp = {};
    wp.addr = lo;
    wp.end = hi;
    wp.type = type;
    char range[24];
    watch_range_text(range, sizeof(range), lo, hi);
    if (!debugger_add_watchpoint(fz_debugger(), wp)) {
        notif_show("Cannot watch %s: no free watchpoint", range);
        return false;
    }
    /* Adding merges the types of an existing watchpoint: set exactly this one */
    debugger_set_watchpoint_type(fz_debugger(), lo, type);
    const char* sym = hi > lo ? nullptr : debugger_get_symbol(fz_debugger(), lo);
    notif_show("Break on %s: %s%s%s", watch_type_name(type), range, sym ? " " : "", sym ? sym : "");
    request_refresh();
    return true;
}

bool watch_remove(hwaddr addr)
{
    if (!dbg_ready() || !debugger_remove_watchpoint(fz_debugger(), addr)) return false;
    char range[24];
    watch_range_text(range, sizeof(range), addr, addr);
    notif_show("Watchpoint at %s removed", range);
    request_refresh();
    return true;
}

bool watch_hit_text(char* out, size_t size, bool brief)
{
    watchpoint_hit_t hit;
    if (!dbg_ready() || !debugger_get_watchpoint_hit(fz_debugger(), &hit)) return false;
    char addr[8];
    char value[8];
    char pc[8];
    hex16(addr, sizeof(addr), hit.addr);
    hex8(value, sizeof(value), hit.value);
    hex16(pc, sizeof(pc), hit.pc);
    if (brief) snprintf(out, size, "%s $%s", watch_type_name(hit.type), addr);
    else snprintf(out, size, "%s $%s = $%s at PC $%s", watch_type_name(hit.type), addr, value, pc);
    return true;
}

} // namespace zui::debugger
