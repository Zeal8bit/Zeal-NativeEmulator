/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <FL/Fl_Button.H>

namespace zui::widgets {

class Segment : public Fl_Button {
public:
    Segment(int X, int Y, int W, int H, const char* label);
    void draw() override;
    int handle(int event) override;
};

} // namespace zui::widgets
