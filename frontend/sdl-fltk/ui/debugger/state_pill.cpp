/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/state_pill.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "theme/theme.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;

StatePill::StatePill(int X, int Y, int W, int H)
    : Fl_Widget(X, Y, W, H)
{}

void StatePill::set(bool paused, uint16_t pc, const char* reason)
{
    m_tip[0] = '\0';
    if (paused && reason != nullptr) {
        /* "Write $C000 = $3E at PC $1234": the pill keeps "WRITE $C000 = $3E" */
        snprintf(m_tip, sizeof(m_tip), "Paused by a watchpoint: %s", reason);
        const char* at = strstr(reason, " at ");
        const int len = at ? (int) (at - reason) : (int) strlen(reason);
        snprintf(m_text, sizeof(m_text), "%.*s", len, reason);
        for (char* t = m_text; *t && *t != ' '; t++) *t = (char) toupper((unsigned char) *t);
    } else if (paused) {
        char addr[8];
        hex16(addr, sizeof(addr), pc);
        snprintf(m_text, sizeof(m_text), "PAUSED $%s", addr);
    } else {
        snprintf(m_text, sizeof(m_text), "RUNNING");
    }
    m_paused = paused;
    tooltip(m_tip[0] ? m_tip : nullptr);
    redraw();
}

void StatePill::draw()
{
    fl_font(DATA_FONT, DATA_SIZE);
    const int tw = (int) fl_width(m_text);
    const int pw = tw + 32;
    const int px = x() + w() - pw;
    const Fl_Color c = m_paused ? COLOR_AMBER : COLOR_GREEN;
    fl_color(COLOR_BG);
    fl_rectf(x(), y(), w(), h());
    fl_color(COLOR_RAISED);
    fl_rectf(px, y() + 2, pw, h() - 4);
    fl_color(c);
    fl_rect(px, y() + 2, pw, h() - 4);
    fl_pie(px + 9, y() + h() / 2 - 4, 8, 8, 0, 360);
    fl_draw(m_text, px + 24, y() + (h() + fl_height()) / 2 - fl_descent());
}

} // namespace zui::debugger
