/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/watch_breaks.h"
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "debugger/watch_model.h"

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

namespace {

/** A register appears inside parentheses: (HL), (IX+2), ((DE))... */
bool follows_register(const std::string& expr)
{
    std::string up;
    for (char c : expr) up += (char) toupper((unsigned char) c);
    static const char* const regs[] = { "HL", "DE", "BC", "SP", "IX", "IY" };
    for (size_t p = up.find('('); p != std::string::npos; p = up.find('(', p + 1)) {
        size_t q = p + 1;
        while (q < up.size() && (up[q] == ' ' || up[q] == '(')) q++;
        for (const char* r : regs) {
            if (up.compare(q, 2, r) != 0) continue;
            const char next = q + 2 < up.size() ? up[q + 2] : ')';
            if (next == ')' || next == '+' || next == '-' || next == ' ') return true;
        }
    }
    return false;
}

int watch_bytes(const Watch& w)
{
    switch (w.type) {
        case W_U16:
        case W_S16:
        case W_PTR8:
        case W_PTR16:
            return 2;
        case W_STR:
            return w.len;
        default:
            return 1;
    }
}

struct Armed {
    hwaddr lo, hi;
    watchpoint_type_t type;
    bool on;
    bool operator==(const Armed& o) const { return lo == o.lo && hi == o.hi && type == o.type && on == o.on; }
};

std::vector<Armed> s_armed;

std::string strip(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && isspace((unsigned char) s[b])) b++;
    while (e > b && isspace((unsigned char) s[e - 1])) e--;
    return s.substr(b, e - b);
}

} // namespace

bool watch_break_target(const Watch& w, hwaddr* lo, hwaddr* hi, const char** why)
{
    if (follows_register(w.expr)) {
        if (why) *why = "its address follows a register";
        return false;
    }
    const regs_t none = {};
    const std::string e = strip(w.expr);
    hwaddr addr;
    if (e.size() > 2 && e.front() == '(' && e.back() == ')') {
        /* (ptr): break on the pointer variable, not on what it points to */
        if (!eval_expr(e.substr(1, e.size() - 2), none, &addr)) {
            if (why) *why = "the expression does not resolve";
            return false;
        }
        *lo = addr;
        *hi = (hwaddr) ((addr + 1) & 0xFFFF);
        if (*hi < *lo) *hi = 0xFFFF; /* never wrap a range around $FFFF */
        return true;
    }
    if (!eval_expr(e, none, &addr)) {
        if (why) *why = "the expression does not resolve";
        return false;
    }
    /* Pointer types: the pointer word itself */
    const int n = is_pointer(w) ? 2 : watch_bytes(w);
    *lo = addr;
    *hi = (hwaddr) ((addr + (hwaddr) n - 1) & 0xFFFF);
    if (*hi < *lo) *hi = 0xFFFF; /* never wrap a range around $FFFF */
    return true;
}

bool watch_break_permanent(const Watch& w)
{
    return follows_register(w.expr);
}

void watch_breaks_sync()
{
    if (!dbg_ready()) return;
    std::vector<Armed> want;
    for (Watch& w : watches()) {
        hwaddr lo, hi;
        if (w.brk == 0) continue;
        if (!watch_break_target(w, &lo, &hi)) {
            /* An address following a register can never break (e.g. the row
             * was edited into (HL)). An unresolved symbol keeps the request,
             * unarmed, until a map provides it. */
            if (watch_break_permanent(w)) w.brk = 0;
            continue;
        }
        want.push_back({ lo, hi, (watchpoint_type_t) w.brk, w.brk_on });
    }
    if (want == s_armed) return;
    s_armed = want;

    /* Core watchpoints are keyed by their start address, so rows sharing
     * bytes (u8 and u16 views of one variable...) cannot each become one.
     * Project the rows per byte instead: what enabled rows request, and
     * where no enabled row covers a byte, what disabled rows request (kept
     * as disabled core entries so the memory view still shows them). */
    std::vector<uint8_t> on(0x10000, 0), off(0x10000, 0);
    for (const Armed& a : want) {
        /* watch_break_target never wraps, so lo <= hi */
        std::vector<uint8_t>& mask = a.on ? on : off;
        for (unsigned addr = a.lo; addr <= a.hi; addr++) mask[addr] |= (uint8_t) a.type;
    }
    auto disabled_at = [&](unsigned addr) { return on[addr] ? 0 : off[addr]; };
    dbg_t* dbg = fz_debugger();
    debugger_clear_watchpoints(dbg);
    for (unsigned addr = 0; addr <= 0xFFFF;) {
        const uint8_t en = on[addr], dis = disabled_at(addr);
        if (en == 0 && dis == 0) {
            addr++;
            continue;
        }
        unsigned end = addr;
        while (end < 0xFFFF && on[end + 1] == en && disabled_at(end + 1) == dis) end++;
        watchpoint_t wp = {};
        wp.addr = (hwaddr) addr;
        wp.end = (hwaddr) end;
        wp.type = (watchpoint_type_t) (en ? en : dis);
        debugger_add_watchpoint(dbg, wp);
        if (en == 0) debugger_enable_watchpoint(dbg, wp.addr, false);
        addr = end + 1;
    }
}

bool watch_break_range(hwaddr lo, hwaddr hi, watchpoint_type_t type)
{
    if (!dbg_ready()) return false;
    std::vector<Watch>& list = watches();
    Watch* row = nullptr;
    for (Watch& w : list) {
        hwaddr a, b;
        if (watch_break_target(w, &a, &b) && a == lo && b == hi && !is_pointer(w)) {
            row = &w;
            break;
        }
    }
    char range[24];
    watch_range_text(range, sizeof(range), lo, hi);
    const int n = (int) (hi - lo) + 1;
    if (row == nullptr) {
        if (n > MAX_STR) {
            notif_show("Break ranges are limited to %d bytes", MAX_STR);
            return false;
        }
        if ((int) list.size() >= MAX_WATCHES) {
            notif_show("Cannot watch %s: the watch list is full", range);
            return false;
        }
        Watch w;
        const char* sym = debugger_get_symbol(fz_debugger(), lo);
        char hex[8];
        hex16(hex, sizeof(hex), lo);
        w.expr = sym && strlen(sym) <= (size_t) MAX_EXPR ? sym : std::string("$") + hex;
        w.type = n == 1 ? W_U8 : n == 2 ? W_U16 : W_STR;
        if (w.type == W_STR) w.len = n;
        list.push_back(w);
        row = &list.back();
    }
    row->brk = (uint8_t) type;
    row->brk_on = true;
    watch_breaks_sync();
    notif_show("Break on %s: %s %s", watch_type_name(type), range, row->expr.c_str());
    request_refresh();
    return true;
}

bool watch_break_clear_at(hwaddr addr)
{
    bool any = false;
    for (Watch& w : watches()) {
        hwaddr lo, hi;
        if (w.brk != 0 && watch_break_target(w, &lo, &hi) && addr >= lo && addr <= hi) {
            w.brk = 0;
            any = true;
        }
    }
    if (!any) return false;
    watch_breaks_sync();
    char range[24];
    watch_range_text(range, sizeof(range), addr, addr);
    notif_show("Break at %s removed", range);
    request_refresh();
    return true;
}

int watch_hit_index()
{
    watchpoint_hit_t hit;
    if (!dbg_ready() || !fz_paused() || !debugger_get_watchpoint_hit(fz_debugger(), &hit)) return -1;
    const std::vector<Watch>& list = watches();
    for (size_t i = 0; i < list.size(); i++) {
        hwaddr lo, hi;
        /* The row must have asked for this kind of access: an overlapping
         * read-only row does not own a write hit */
        if (list[i].brk != 0 && list[i].brk_on && (list[i].brk & hit.type) != 0 &&
            watch_break_target(list[i], &lo, &hi) && hit.addr >= lo && hit.addr <= hi) {
            return (int) i;
        }
    }
    return -1;
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
