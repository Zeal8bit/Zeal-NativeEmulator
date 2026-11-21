/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Segmented mode switch, styled like the tool window tabs */

#pragma once

#include <FL/Fl_Button.H>

namespace zui::widgets {

class ModeButton : public Fl_Button {
public:
    ModeButton(int X, int Y, int W, int H, const char* label);
    void draw() override;
    int handle(int event) override;
};

} // namespace zui::widgets
