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
    void set(bool paused, uint16_t pc);
    void draw() override;

private:
    char m_text[32] = "RUNNING";
    bool m_paused = false;
};

} // namespace zui::debugger
