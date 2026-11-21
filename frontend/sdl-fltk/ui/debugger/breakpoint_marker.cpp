/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/breakpoint_marker.h"
#include <FL/fl_draw.H>
#include "theme/theme.h"

namespace zui::debugger {

using namespace zui::theme;

void draw_breakpoint_marker(int X, int Y, int size, bool enabled)
{
    fl_color(COLOR_BREAK);
    if (enabled) {
        fl_pie(X, Y, size, size, 0, 360);
    } else {
        fl_line_style(FL_SOLID, 2);
        fl_arc(X + 1, Y + 1, size - 2, size - 2, 0, 360);
        fl_line_style(0);
    }
}

} // namespace zui::debugger
