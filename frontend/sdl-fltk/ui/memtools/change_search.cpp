/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "memtools/change_search.h"
#include <cstddef>
#include <cstdint>

namespace zui::memtools {

void ChangeSearch::start(int size)
{
    m_candidates.resize((size_t) size);
    for (int i = 0; i < size; i++) m_candidates[(size_t) i] = (uint16_t) i;
    m_active = true;
    m_rounds = 0;
}

bool ChangeSearch::passes(Filter filter, uint8_t value, uint8_t prev, uint8_t cur)
{
    switch (filter) {
        case Filter::Changed:   return cur != prev;
        case Filter::Unchanged: return cur == prev;
        case Filter::Increased: return cur > prev;
        case Filter::Decreased: return cur < prev;
        case Filter::Equal:     return cur == value;
    }
    return false;
}

void ChangeSearch::narrow(Filter filter, uint8_t value, const uint8_t* prev, const uint8_t* cur)
{
    if (!m_active) {
        start();
    }
    size_t keep = 0;
    for (size_t i = 0; i < m_candidates.size(); i++) {
        const uint16_t a = m_candidates[i];
        if (passes(filter, value, prev ? prev[a] : cur[a], cur[a])) {
            m_candidates[keep++] = a;
        }
    }
    m_candidates.resize(keep);
    m_rounds++;
}

} // namespace zui::memtools
