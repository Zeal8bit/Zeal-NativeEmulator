/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/watch_model.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/address_lookup.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "memtools/pattern.h"

namespace zui::debugger {

using namespace zui::memtools;
using namespace zui::common;

namespace {

std::string trim(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && isspace((unsigned char) s[b])) b++;
    while (e > b && isspace((unsigned char) s[e - 1])) e--;
    return s.substr(b, e - b);
}

/** Offset: hex by default ($/0x prefixes accepted), decimal with '#'. */
bool parse_offset(const std::string& text, long* out)
{
    std::string s = trim(text);
    int base = 16;
    size_t i = 0;
    if (!s.empty() && s[0] == '#') {
        base = 10;
        i = 1;
    } else if (!s.empty() && s[0] == '$') {
        i = 1;
    } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        i = 2;
    }
    if (i >= s.size()) return false;
    char* end = nullptr;
    const long v = strtol(s.c_str() + i, &end, base);
    if (end == s.c_str() + i || *end != '\0') return false;
    *out = v;
    return true;
}

void read_mem(hwaddr addr, int len, uint8_t* out)
{
    addr &= 0xFFFF;
    int first = 0x10000 - (int) addr;
    if (first > len) first = len;
    debugger_read_memory(fz_debugger(), addr, first, out);
    if (first < len) debugger_read_memory(fz_debugger(), 0, len - first, out + first);
}

} // namespace

const TypeInfo WATCH_TYPES[W_COUNT] = {
    { "u8", "u8", "u8   unsigned byte" },
    { "s8", "s8", "s8   signed byte" },
    { "u16", "u16", "u16  unsigned word (LE)" },
    { "s16", "s16", "s16  signed word (LE)" },
    { "string", "str", "str  ASCII string" },
    { "ptr8", "*u8", "*u8  pointer to byte" },
    { "ptr16", "*u16", "*u16 pointer to word" },
    { "bits", "bits", "bits bitfield (8 bits)" },
};

/* The list lives outside the panel so that it can be saved at exit even
 * after the debugger window has been destroyed. */
std::vector<Watch>& watches()
{
    static std::vector<Watch> list;
    return list;
}

/** Address designated by an expression (see the top of the file). */
bool eval_expr(const std::string& text, const regs_t& regs, hwaddr* out, int depth)
{
    const std::string e = trim(text);
    if (e.empty() || depth > 3) return false;

    if (e.front() == '(' && e.back() == ')') {
        const std::string in = trim(e.substr(1, e.size() - 2));
        std::string up = in;
        for (char& c : up) c = (char) toupper((unsigned char) c);
        const struct {
            const char* name;
            uint16_t value;
        } regs16[] = {
            { "HL", regs.hl }, { "DE", regs.de }, { "BC", regs.bc },
            { "SP", regs.sp }, { "IX", regs.ix }, { "IY", regs.iy },
        };
        for (const auto& r : regs16) {
            if (up.compare(0, 2, r.name) != 0) continue;
            const std::string rest = trim(in.substr(2));
            long off = 0;
            if (!rest.empty()) {
                if (rest[0] != '+' && rest[0] != '-') break; /* not a register */
                if (!parse_offset(rest.substr(1), &off)) return false;
                if (rest[0] == '-') off = -off;
            }
            *out = (hwaddr) ((r.value + off) & 0xFFFF);
            return true;
        }
        /* Memory indirection: the little-endian word stored at the inner address */
        hwaddr ptr;
        if (!eval_expr(in, regs, &ptr, depth + 1)) return false;
        uint8_t b[2];
        read_mem(ptr, 2, b);
        *out = (hwaddr) (b[0] | (b[1] << 8));
        return true;
    }

    hwaddr addr;
    if (parse_address(e.c_str(), &addr)) {
        *out = addr;
        return true;
    }
    const size_t pos = e.find_last_of("+-");
    if (pos == std::string::npos || pos == 0) return false;
    long off;
    if (!parse_offset(e.substr(pos + 1), &off)) return false;
    if (!eval_expr(e.substr(0, pos), regs, &addr, depth + 1)) return false;
    if (e[pos] == '-') off = -off;
    *out = (hwaddr) ((addr + off) & 0xFFFF);
    return true;
}

void evaluate(Watch& w, const regs_t& regs)
{
    w.valid = eval_expr(w.expr, regs, &w.addr);
    if (!w.valid) {
        w.n = 0;
        return;
    }
    switch (w.type) {
        case W_U8:
        case W_S8:
        case W_BITS:
            w.n = 1;
            break;
        case W_U16:
        case W_S16:
            w.n = 2;
            break;
        case W_STR:
            w.n = w.len;
            break;
        case W_PTR8:
        case W_PTR16:
            w.n = 2;
            break;
        default:
            w.n = 1;
            break;
    }
    read_mem(w.addr, w.n, w.cur);
    if (w.type == W_PTR8 || w.type == W_PTR16) {
        w.target = (hwaddr) (w.cur[0] | (w.cur[1] << 8));
        const int tn = w.type == W_PTR8 ? 1 : 2;
        read_mem(w.target, tn, w.cur + 2);
        w.n = 2 + tn;
    }
}

bool is_pointer(const Watch& w)
{
    return w.type == W_PTR8 || w.type == W_PTR16;
}

/** Address shown in the ADDR column: where the displayed value lives
 *  (the target for pointers, whose own location is the expression). */
hwaddr shown_addr(const Watch& w)
{
    return is_pointer(w) ? w.target : w.addr;
}

/** Remember the current value as the reference for change highlighting. */
void take_snapshot(Watch& w)
{
    memcpy(w.prev, w.cur, sizeof(w.prev));
    w.prev_n = w.n;
    w.prev_addr = shown_addr(w);
    w.has_prev = w.valid;
}

/** HEX and DEC cell texts. Strings use `hex` only (quoted text). */
void format(const Watch& w, char* hex, size_t hs, char* dec, size_t ds)
{
    hex[0] = dec[0] = '\0';
    if (!w.valid) {
        snprintf(hex, hs, "?");
        return;
    }
    const unsigned b = w.cur[0];
    const unsigned word = w.cur[0] | (w.cur[1] << 8);
    switch (w.type) {
        case W_U8:
            hex8(hex, hs, b);
            snprintf(dec, ds, "%u", b);
            break;
        case W_S8:
            hex8(hex, hs, b);
            snprintf(dec, ds, "%d", (int) (int8_t) b);
            break;
        case W_U16:
            hex16(hex, hs, word);
            snprintf(dec, ds, "%u", word);
            break;
        case W_S16:
            hex16(hex, hs, word);
            snprintf(dec, ds, "%d", (int) (int16_t) word);
            break;
        case W_BITS:
            hex8(hex, hs, b); /* bits drawn by the list (draw_bits) */
            break;
        case W_PTR8: /* the pointer itself is shown in the ADDR column */
            hex8(hex, hs, w.cur[2]);
            snprintf(dec, ds, "%u", (unsigned) w.cur[2]);
            break;
        case W_PTR16: {
            const unsigned tw = w.cur[2] | (w.cur[3] << 8);
            hex16(hex, hs, tw);
            snprintf(dec, ds, "%u", tw);
            break;
        }
        case W_STR: {
            size_t o = 0;
            if (o + 1 < hs) hex[o++] = '"';
            for (int i = 0; i < w.n && o + 2 < hs; i++) {
                const uint8_t c = w.cur[i];
                if (c == 0) break; /* C string terminator */
                hex[o++] = (c >= 0x20 && c < 0x7F) ? (char) c : '.';
            }
            if (o + 1 < hs) hex[o++] = '"';
            hex[o] = '\0';
            break;
        }
        default:
            break;
    }
}

bool watch_add_at(hwaddr addr, int type, int len)
{
    std::vector<Watch>& list = watches();
    if ((int) list.size() >= MAX_WATCHES) {
        notif_show("The watch list is full (%d entries)", MAX_WATCHES);
        return false;
    }
    Watch w;
    const char* sym = dbg_ready() ? debugger_get_symbol(fz_debugger(), addr) : nullptr;
    char hex[8];
    hex16(hex, sizeof(hex), addr);
    w.expr = sym && strlen(sym) <= (size_t) MAX_EXPR && !strchr(sym, '|') ? sym : std::string("$") + hex;
    w.type = type >= 0 && type < W_COUNT ? type : W_U8;
    if (len >= 1 && len <= MAX_STR) w.len = len;
    list.push_back(w);
    notif_show("Watching %s as %s", w.expr.c_str(), WATCH_TYPES[w.type].tag);
    request_refresh();
    return true;
}

} // namespace zui::debugger
