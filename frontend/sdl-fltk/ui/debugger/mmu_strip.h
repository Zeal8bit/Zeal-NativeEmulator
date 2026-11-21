/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** The four 16 KB pages of the Z80 address space and what is mapped there. */

#pragma once

#include <FL/Fl_Widget.H>
#include "common/machine.h"

extern "C" {
#include "debugger/zeal_debugger.h"
}

namespace zui::debugger {

class MmuStrip : public Fl_Widget {
public:
    MmuStrip(int X, int Y, int W, int H);
    void refresh();
    void draw() override;
    int handle(int event) override;

private:
    static Fl_Color device_color(const char* name);

    dbg_mmu_t m_info = {};
    bool m_valid = false;
};

} // namespace zui::debugger
