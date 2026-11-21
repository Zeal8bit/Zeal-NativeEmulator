/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "widgets/panel.h"

namespace zui::profiler {

class SemihostView : public widgets::Panel {
public:
    SemihostView(int X, int Y, int W, int H);
    void refresh(bool paused, bool force) override;
    void draw() override;
    int handle(int event) override;

private:
    int row_at(int ey) const;

    int m_rows_y = 0;
    int m_row_h = 0;
    int m_hover = -1;
};

} // namespace zui::profiler
