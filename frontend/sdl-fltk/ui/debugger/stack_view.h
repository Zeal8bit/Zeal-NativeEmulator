/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include "common/machine.h"
#include "widgets/panel.h"

namespace zui::debugger {

class StackView : public widgets::Panel {
public:
    StackView(int X, int Y, int W, int H);
    void refresh(bool paused, bool force) override;
    void draw() override;
    int handle(int event) override;

private:
    /** Words from SP up to the top of memory (or the sample while running). */
    int stack_limit() const;
    void clamp_first(int rows, int limit);
    int row_at(int ey) const;
    void context_menu(hwaddr slot, uint16_t value);

    regs_t m_regs = {};
    int m_hover = -1;
    int m_first = 0; /* first word shown, relative to SP (mouse wheel) */
    uint16_t m_words[128] = {};
    int m_count = 0; /* rows drawn */
};

} // namespace zui::debugger
