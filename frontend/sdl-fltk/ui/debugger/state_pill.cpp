/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/state_pill.h"
#include <algorithm>
#include <cctype>
#include "common/machine.h"
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
    m_tip.clear();
    if (paused && reason != nullptr) {
        /* "Write $C000 = $3E at PC $1234": the pill keeps "WRITE $C000 = $3E" */
        m_tip = std::string("Paused by a watchpoint: ") + reason;
        const char* at = strstr(reason, " at ");
        const int len = at ? (int) (at - reason) : (int) strlen(reason);
        m_text.assign(reason, len);
        for (char& c : m_text) {
            if (c == ' ') {
                break;
            }
            c = (char) toupper((unsigned char) c);
        }
    } else if (paused) {
        char addr[8];
        dbg_t* dbg = fz_debugger();
        if (dbg && dbg->breakpoint_stopped) {
            pc = dbg->breakpoint_hit_addr;
        }
        hex16(addr, sizeof(addr), pc);
        m_text = dbg && dbg->breakpoint_stopped ? "BREAKPOINT $" : "PAUSED $";
        m_text += addr;
        if (const char* symbol = debugger_get_symbol(dbg, pc)) {
            m_text += " ";
            m_text += symbol;
        }
        m_tip = m_text;
    } else {
        m_text = "RUNNING";
    }
    m_paused = paused;
    tooltip(m_tip.empty() ? nullptr : m_tip.c_str());
    redraw();
}

void StatePill::draw()
{
    fl_font(DATA_FONT, DATA_SIZE);
    const int tw = (int) fl_width(m_text.c_str());
    const int pw = std::min(w(), tw + 32);
    const int px = x() + w() - pw;
    const Fl_Color c = m_paused ? COLOR_AMBER : COLOR_GREEN;
    fl_color(COLOR_BG);
    fl_rectf(x(), y(), w(), h());
    fl_color(COLOR_RAISED);
    fl_rectf(px, y() + 2, pw, h() - 4);
    fl_color(c);
    fl_rect(px, y() + 2, pw, h() - 4);
    fl_pie(px + 9, y() + h() / 2 - 4, 8, 8, 0, 360);
    fl_push_clip(px, y(), pw, h());
    fl_draw(m_text.c_str(), px + 24, y() + (h() + fl_height()) / 2 - fl_descent());
    fl_pop_clip();
}

} // namespace zui::debugger
