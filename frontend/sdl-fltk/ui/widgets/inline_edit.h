/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** Inline editor that cancels on Escape or focus loss. */

#pragma once

#include <FL/Fl_Input.H>

namespace zui::widgets {

class InlineEdit : public Fl_Input {
public:
    InlineEdit(int X, int Y, int W, int H);
    int handle(int event) override;
};

} // namespace zui::widgets
