/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** Search field of the memory panel: Enter = next, Shift+Enter = previous,
 *  Escape closes the search row. */

#pragma once

#include <functional>
#include <FL/Fl_Input.H>

namespace zui::debugger {

class MemFindInput : public Fl_Input {
public:
    MemFindInput(int X, int Y, int W, int H);

    std::function<void(bool forward)> on_enter;
    std::function<void()> on_escape;

    int handle(int event) override;
};

} // namespace zui::debugger
