/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* "Changed since last pause" candidate search of the memory editor. Plain
 * C++ with no FLTK or machine dependency. */
#pragma once

#include <cstdint>
#include <vector>

namespace zui::memtools {

enum class Filter { Changed, Unchanged, Increased, Decreased, Equal };

class ChangeSearch {
public:
    /** Begin a new search: every address of `size` is a candidate. */
    void start(int size = 0x10000);
    void stop() { m_active = false; m_candidates.clear(); m_rounds = 0; }
    bool active() const { return m_active; }
    /** Keep the candidates for which `prev` -> `cur` passes the filter. */
    void narrow(Filter filter, uint8_t value, const uint8_t* prev, const uint8_t* cur);
    const std::vector<uint16_t>& candidates() const { return m_candidates; }
    int rounds() const { return m_rounds; }

    static bool passes(Filter filter, uint8_t value, uint8_t prev, uint8_t cur);

private:
    std::vector<uint16_t> m_candidates;
    bool m_active = false;
    int m_rounds = 0;
};

} // namespace zui::memtools
