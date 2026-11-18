/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "trace/symbol_index.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "common/format.h"
#include "common/machine.h"

namespace zui::trace {

using namespace zui::common;

namespace {

constexpr hwaddr MAX_OFFSET = 0x1000;

} // namespace

uintptr_t SymbolIndex::signature()
{
    dbg_t* dbg = fz_debugger();
    if (dbg == nullptr) return 0;
    uintptr_t sig = 0;
    for (const symbols_t* s = &dbg->symbols; s != nullptr; s = s->next) {
        sig = sig * 31 + s->count;
        if (s->count > 0) sig ^= (uintptr_t) s->array[0].name;
    }
    return sig;
}

bool SymbolIndex::update()
{
    const uintptr_t sig = signature();
    if (sig == m_sig) return false;
    m_sig = sig;
    m_syms.clear();
    dbg_t* dbg = fz_debugger();
    if (dbg == nullptr) return true;
    for (const symbols_t* s = &dbg->symbols; s != nullptr; s = s->next) {
        for (unsigned i = 0; i < s->count && i < DBG_SYM_COUNT; i++) {
            const symbol_t& sym = s->array[i];
            if (sym.name == nullptr || sym.addr > 0xFFFF) continue;
            /* Variables and constants never contain code */
            if (sym.kind == SYM_KIND_DATA || sym.kind == SYM_KIND_CONST) continue;
            m_syms.push_back({ sym.addr, sym.name });
        }
    }
    std::stable_sort(m_syms.begin(), m_syms.end(),
                     [](const Sym& a, const Sym& b) { return a.addr < b.addr; });
    /* One name per address: keep the first one loaded */
    m_syms.erase(std::unique(m_syms.begin(), m_syms.end(),
                             [](const Sym& a, const Sym& b) { return a.addr == b.addr; }),
                 m_syms.end());
    return true;
}

void SymbolIndex::describe(char* out, size_t size, hwaddr addr) const
{
    if (size == 0) return;
    out[0] = 0;
    auto it = std::upper_bound(m_syms.begin(), m_syms.end(), addr,
                               [](hwaddr a, const Sym& s) { return a < s.addr; });
    if (it == m_syms.begin()) return;
    --it;
    const hwaddr off = addr - it->addr;
    /* Too far to be the same routine (sparse map, or code the map doesn't cover) */
    if (off > MAX_OFFSET) return;
    if (off == 0) {
        snprintf(out, size, "%s", it->name);
        return;
    }
    char h[8];
    if (off < 0x100) hex8(h, sizeof(h), off);
    else hex16(h, sizeof(h), off);
    snprintf(out, size, "%s+$%s", it->name, h);
}

} // namespace zui::trace
