/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** Filter field that clears itself on Escape. */

#pragma once

#include <FL/Fl_Input.H>

namespace zui::widgets {

class FilterInput : public Fl_Input {
public:
    FilterInput(int X, int Y, int W, int H);
    int handle(int event) override;
};

} // namespace zui::widgets
