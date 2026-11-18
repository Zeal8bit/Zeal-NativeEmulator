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
#include <string>
#include <vector>
#include "common/machine.h"

namespace zui::trace {

class SymbolIndex {
public:
    explicit SymbolIndex(bool code_only = true) : m_code_only(code_only) {}

    /** Reload if the symbol table changed; true when it did. */
    bool update();
    /** "name", "name+$1A" or "" when no code symbol precedes `addr` within 4 KB. */
    void describe(char* out, size_t size, hwaddr addr) const;

    struct Sym {
        hwaddr addr;
        std::string name;
    };

    const std::vector<Sym>& symbols() const { return m_syms; }
    int preceding(hwaddr addr) const;

private:
    bool m_code_only;
    std::vector<Sym> m_syms;
    uint64_t m_generation = UINT64_MAX;    /* force the first load */
};

} // namespace zui::trace
