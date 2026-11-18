/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/stack_view.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "theme/theme.h"
#include "widgets/panel.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;
using namespace zui::widgets;

StackView::StackView(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "STACK")
{
    resizable(nullptr);
    end();
    tooltip("Click: show the slot in memory    Right-click: more");
}

void StackView::refresh(bool paused, bool force)
{
    (void) paused;
    (void) force;
    m_regs = current_regs();
    char text[32];
    char sp[8];
    hex16(sp, sizeof(sp), m_regs.sp);
    if (m_first > 0) snprintf(text, sizeof(text), "SP $%s  +%X", sp, m_first * 2);
    else snprintf(text, sizeof(text), "SP $%s", sp);
    extra(text);
    redraw();
}

void StackView::draw()
{
    Panel::draw();
    if (!dbg_ready()) return;
    const int lh = data_line_height();
    const int cw = char_width();
    const int asc = text_baseline(lh);
    const int rows = (h() - HEADER_H - 8) / lh;
    /* Only whole words between SP and the end of the address space can be
     * stack (it grows down; never wrap to $0000); while running, only the
     * sampled 32 words exist. Never scroll past a full view. */
    const int limit = stack_limit();
    clamp_first(rows, limit);
    int count = rows < 128 ? rows : 128;
    if (m_first + count > limit) count = limit - m_first;
    m_count = count > 0 ? count : 0;
    if (limit <= 0) {
        fl_font(FL_HELVETICA, 11);
        fl_color(COLOR_DIM);
        fl_draw("Empty: SP is at the top of memory.", x() + 10, top() + 4, w() - 20, lh,
                FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_CLIP);
        return;
    }
    if (count <= 0) return;
    uint8_t mem[256 + 64];
    if (fz_paused()) {
        debugger_read_memory(fz_debugger(), (m_regs.sp + m_first * 2) & 0xFFFF, count * 2, mem);
    } else {
        uint8_t sample[64];
        display_memory(m_regs.sp, 64, sample, true);
        memcpy(mem, sample + m_first * 2, (size_t) count * 2);
    }
    fl_push_clip(x(), top(), w(), h() - HEADER_H);
    fl_font(DATA_FONT, DATA_SIZE);
    char text[16];
    for (int i = 0; i < count; i++) {
        const int ry = top() + 4 + i * lh;
        const int index = m_first + i;
        const unsigned addr = (m_regs.sp + index * 2) & 0xFFFF;
        const unsigned word = mem[i * 2] | (mem[i * 2 + 1] << 8);
        if (i < (int) (sizeof(m_words) / sizeof(m_words[0]))) m_words[i] = (uint16_t) word;
        if (i == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(x(), ry, w(), lh);
        }
        if (index == 0) {
            fl_color(COLOR_AMBER);
            fl_draw(">", x() + 6, ry + asc);
        }
        fl_color(COLOR_DIM);
        snprintf(text, sizeof(text), index * 2 < 0x100 ? "+%02X" : "+%X", index * 2);
        fl_draw(text, x() + 6 + cw * 2, ry + asc);
        hex16(text, sizeof(text), addr);
        fl_color(COLOR_CYAN);
        fl_draw(text, x() + 6 + cw * 6, ry + asc);
        hex16(text, sizeof(text), word);
        fl_color(COLOR_TEXT);
        fl_draw(text, x() + 6 + cw * 12, ry + asc);
        const char* sym = debugger_get_symbol(fz_debugger(), word);
        if (sym) {
            fl_color(COLOR_AMBER);
            fl_draw(sym, x() + 6 + cw * 17, ry + asc);
        }
    }
    fl_pop_clip();
}

int StackView::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            m_hover = -1;
            redraw();
            return 1;
        case FL_MOVE: {
            const int r = row_at(Fl::event_y());
            if (r != m_hover) {
                m_hover = r;
                redraw();
            }
            return 1;
        }
        case FL_MOUSEWHEEL:
            m_first += Fl::event_dy() * 2;
            clamp_first((h() - HEADER_H - 8) / data_line_height(), stack_limit());
            {
                char text[32], sp[8];
                hex16(sp, sizeof(sp), m_regs.sp);
                if (m_first > 0) snprintf(text, sizeof(text), "SP $%s  +%X", sp, m_first * 2);
                else snprintf(text, sizeof(text), "SP $%s", sp);
                extra(text);
            }
            redraw();
            return 1;
        case FL_PUSH: {
            const int r = row_at(Fl::event_y());
            if (r < 0 || !dbg_ready()) return 1;
            const hwaddr addr = (m_regs.sp + (m_first + r) * 2) & 0xFFFF;
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                context_menu(addr, m_words[r]);
            } else {
                goto_memory(addr);
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Group::handle(event);
}

int StackView::stack_limit() const
{
    const int words = (0x10000 - (int) m_regs.sp) / 2;
    const int sampled = fz_paused() ? words : 32;
    return words < sampled ? words : sampled;
}

void StackView::clamp_first(int rows, int limit)
{
    int max_first = limit - (rows < limit ? rows : limit);
    if (max_first < 0) max_first = 0;
    if (m_first > max_first) m_first = max_first;
    if (m_first < 0) m_first = 0;
}

int StackView::row_at(int ey) const
{
    if (ey < top() + 4) return -1;
    const int r = (ey - top() - 4) / data_line_height();
    return r < m_count ? r : -1;
}

void StackView::context_menu(hwaddr slot, uint16_t value)
{
    Fl_Menu_Item items[] = {
        { "Show Slot in Memory", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Show Value in Memory", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Show Value in Disassembly", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Copy Value", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Copy Address", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) return;
    char text[8];
    switch ((int) (picked - items)) {
        case 0: goto_memory(slot); break;
        case 1: goto_memory(value); break;
        case 2: goto_disassembly(value); break;
        case 3:
            hex16(text, sizeof(text), value);
            Fl::copy(text, (int) strlen(text), 1);
            break;
        case 4:
            hex16(text, sizeof(text), slot);
            Fl::copy(text, (int) strlen(text), 1);
            break;
        default: break;
    }
}

} // namespace zui::debugger
