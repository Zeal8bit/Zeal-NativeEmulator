/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <FL/Fl_Widget.H>

namespace zui::debugger {

class StatePill : public Fl_Widget {
public:
    StatePill(int X, int Y, int W, int H);
    /** `reason` (a watchpoint hit) replaces "PAUSED $pc", in full as the tooltip. */
    void set(bool paused, uint16_t pc, const char* reason = nullptr);
    void draw() override;

private:
    char m_text[32] = "RUNNING";
    char m_tip[96] = "";
    bool m_paused = false;
};

} // namespace zui::debugger
