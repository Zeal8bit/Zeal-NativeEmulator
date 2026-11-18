/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Code symbols sorted by address, to name any address as "symbol+$off"
 * (the nearest preceding code symbol, at most 4 KB before). Reloads itself
 * when the debugger's symbol table changes (map loaded, reloaded or cleared). */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "common/machine.h"

namespace zui::trace {

class SymbolIndex {
public:
    /** Reload if the symbol table changed; true when it did. */
    bool update();
    /** "name", "name+$1A" or "" when no code symbol precedes `addr` within 4 KB. */
    void describe(char* out, size_t size, hwaddr addr) const;

private:
    struct Sym {
        hwaddr addr;
        const char* name;
    };
    static uintptr_t signature();

    std::vector<Sym> m_syms;
    uintptr_t m_sig = 1;    /* force the first load */
};

} // namespace zui::trace
