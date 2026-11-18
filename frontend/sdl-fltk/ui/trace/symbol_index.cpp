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

bool SymbolIndex::update()
{
    dbg_t* dbg = fz_debugger();
    const uint64_t generation = dbg ? dbg->symbol_generation : 0;
    if (generation == m_generation) return false;
    m_generation = generation;
    m_syms.clear();
    if (dbg == nullptr) return true;
    for (const symbols_t* s = &dbg->symbols; s != nullptr; s = s->next) {
        for (unsigned i = 0; i < s->count && i < DBG_SYM_COUNT; i++) {
            const symbol_t& sym = s->array[i];
            if (sym.name == nullptr || sym.addr > 0xFFFF) continue;
            /* Variables and constants never contain code */
            if (m_code_only && (sym.kind == SYM_KIND_DATA || sym.kind == SYM_KIND_CONST)) continue;
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

int SymbolIndex::preceding(hwaddr addr) const
{
    auto it = std::upper_bound(m_syms.begin(), m_syms.end(), addr,
                               [](hwaddr a, const Sym& s) { return a < s.addr; });
    return (int) (it - m_syms.begin()) - 1;
}

void SymbolIndex::describe(char* out, size_t size, hwaddr addr) const
{
    if (size == 0) return;
    out[0] = 0;
    const int idx = preceding(addr);
    if (idx < 0) return;
    const Sym& sym = m_syms[(size_t) idx];
    const hwaddr off = addr - sym.addr;
    /* Too far to be the same routine (sparse map, or code the map doesn't cover) */
    if (off > MAX_OFFSET) return;
    if (off == 0) {
        snprintf(out, size, "%s", sym.name.c_str());
        return;
    }
    char h[8];
    if (off < 0x100) hex8(h, sizeof(h), off);
    else hex16(h, sizeof(h), off);
    snprintf(out, size, "%s+$%s", sym.name.c_str(), h);
}

} // namespace zui::trace
