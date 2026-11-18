/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/disasm_view.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/breakpoint_marker.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "theme/theme.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;

DisasmView::DisasmView(int X, int Y, int W, int H)
    : Fl_Widget(X, Y, W, H)
{}

void DisasmView::go(hwaddr addr)
{
    m_base = addr & 0xFFFF;
    m_history.clear();
    redraw();
}

void DisasmView::follow(hwaddr pc)
{
    for (size_t i = 0; i + 3 < m_rows.size(); i++) {
        if (m_rows[i].addr == pc) return;
    }
    go(pc);
}

void DisasmView::draw()
{
    fl_push_clip(x(), y(), w(), h());
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    fl_color(COLOR_BG);
    fl_rectf(x(), y(), GUTTER, h());
    if (!dbg_ready()) {
        fl_pop_clip();
        return;
    }
    dbg_t* dbg = fz_debugger();
    const regs_t regs = current_regs();
    const bool paused = fz_paused();
    const int lh = data_line_height();
    const int cw = char_width();
    const int asc = text_baseline(lh);
    const int x_addr = x() + GUTTER + 6;
    const int x_bytes = x_addr + cw * 6;
    const int x_instr = x_bytes + cw * 12;
    fl_font(DATA_FONT, DATA_SIZE);

    m_rows.clear();
    hwaddr addr = m_base;
    int yy = y() + 4;
    char buf[96];
    while (yy + lh <= y() + h() && addr <= 0xFFFF) {
        dbg_instr_t instr;
        memset(&instr, 0, sizeof(instr));
        const int size = debugger_disassemble_address(dbg, addr, &instr);
        if (instr.label[0]) {
            if (yy > y() + 4) yy += 4;
            fl_color(COLOR_AMBER);
            snprintf(buf, sizeof(buf), "%s:", instr.label);
            fl_draw(buf, x_addr, yy + asc);
            yy += lh;
            if (yy + lh > y() + h()) break;
        }

        const bool is_pc = paused && addr == regs.pc;
        const bool is_bp = debugger_has_breakpoint(dbg, addr);
        const bool bp_on = is_bp && debugger_is_breakpoint_enabled(dbg, addr);
        if (is_pc) {
            fl_color(COLOR_PC);
            fl_rectf(x() + GUTTER, yy, w() - GUTTER, lh);
            fl_color(COLOR_GREEN);
            fl_rectf(x() + GUTTER, yy, 2, lh);
        } else if ((int) addr == selected) {
            fl_color(COLOR_SELECT);
            fl_rectf(x() + GUTTER, yy, w() - GUTTER, lh);
        } else if ((int) addr == m_hover) {
            fl_color(COLOR_HOVER);
            fl_rectf(x() + GUTTER, yy, w() - GUTTER, lh);
        }
        if (is_bp) {
            draw_breakpoint_marker(x() + 5, yy + (lh - 10) / 2, 10, bp_on);
        } else if ((int) addr == m_hover) {
            fl_color(COLOR_BORDER);
            fl_pie(x() + 5, yy + (lh - 10) / 2, 10, 10, 0, 360);
        }
        if (is_pc) {
            fl_color(COLOR_GREEN);
            const int cy = yy + lh / 2;
            fl_polygon(x() + 11, cy - 5, x() + 19, cy, x() + 11, cy + 5);
        }

        hex16(buf, sizeof(buf), addr);
        fl_color(is_pc ? COLOR_GREEN : COLOR_CYAN);
        fl_draw(buf, x_addr, yy + asc);

        const int n = size > 0 ? (size > 4 ? 4 : size) : 1;
        int bx = 0;
        for (int i = 0; i < n; i++) {
            bx += snprintf(buf + bx, sizeof(buf) - bx, hex8_fmt(), instr.opcodes[i]);
            buf[bx++] = ' ';
            buf[bx] = '\0';
        }
        fl_color(COLOR_DIM);
        fl_draw(buf, x_bytes, yy + asc);

        fl_color(is_pc ? COLOR_GREEN : COLOR_TEXT);
        fl_draw(instr.instruction, x_instr, yy + asc);

        m_rows.push_back({ yy, lh, addr });
        yy += lh;
        if (size <= 0) break;
        addr += size;
    }
    fl_pop_clip();
}

int DisasmView::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            m_hover = -1;
            redraw();
            return 1;
        case FL_MOVE: {
            const int hover = row_at(Fl::event_y());
            if (hover != m_hover) {
                m_hover = hover;
                redraw();
            }
            return 1;
        }
        case FL_PUSH: {
            const int addr = row_at(Fl::event_y());
            if (addr < 0) return 1;
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                selected = addr;
                redraw();
                context_menu((hwaddr) addr);
                return 1;
            }
            /* Click = primary action: the gutter toggles a breakpoint, the
             * rest of the line selects it (for Run to). Right-click: menu. */
            if (Fl::event_x() < x() + GUTTER) {
                debugger_toggle_breakpoint(fz_debugger(), (hwaddr) addr);
                request_refresh();
            } else {
                selected = addr;
                redraw();
            }
            return 1;
        }
        case FL_MOUSEWHEEL: {
            const int dy = Fl::event_dy();
            const int steps = (dy > 0 ? dy : -dy) * 3;
            for (int i = 0; i < steps; i++) {
                if (dy > 0) {
                    dbg_instr_t instr;
                    const int size = debugger_disassemble_address(fz_debugger(), m_base, &instr);
                    const hwaddr next = (hwaddr) (m_base + (size > 0 ? size : 1));
                    if (next > 0xFFFF) break;
                    m_history.push_back(m_base);
                    m_base = next;
                } else if (!m_history.empty()) {
                    m_base = m_history.back();
                    m_history.pop_back();
                } else if (m_base > 0) {
                    m_base--;
                }
            }
            redraw();
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

int DisasmView::row_at(int ey) const
{
    for (const Row& r : m_rows) {
        if (ey >= r.y && ey < r.y + r.h) return (int) r.addr;
    }
    return -1;
}

void DisasmView::context_menu(hwaddr addr)
{
    const int pause_only = fz_paused() ? 0 : FL_MENU_INACTIVE;
    dbg_t* bp_dbg = fz_debugger();
    const bool has_bp = debugger_has_breakpoint(bp_dbg, addr);
    const bool bp_on = has_bp && debugger_is_breakpoint_enabled(bp_dbg, addr);
    Fl_Menu_Item items[] = {
        { "Run to Cursor", 0, nullptr, nullptr, pause_only, 0, 0, 0, 0 },
        { "Set PC Here", 0, nullptr, nullptr, pause_only | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { has_bp ? "Remove Breakpoint" : "Add Breakpoint", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { bp_on ? "Disable Breakpoint" : "Enable Breakpoint", 0, nullptr, nullptr,
          (has_bp ? 0 : FL_MENU_INACTIVE) | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Show in Memory", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Copy Address", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) return;
    dbg_t* dbg = fz_debugger();
    char text[8];
    switch ((int) (picked - items)) {
        case 0:
            debugger_set_temporary_breakpoint(dbg, addr);
            debugger_continue(dbg);
            break;
        case 1: {
            regs_t regs = exact_regs();
            regs.pc = (uint16_t) addr;
            debugger_set_registers(dbg, &regs);
            break;
        }
        case 2:
            debugger_toggle_breakpoint(dbg, addr);
            break;
        case 3:
            debugger_enable_breakpoint(dbg, addr, !bp_on);
            break;
        case 4:
            goto_memory(addr);
            break;
        case 5:
            hex16(text, sizeof(text), addr);
            Fl::copy(text, (int) strlen(text), 1);
            break;
        default:
            break;
    }
    request_refresh();
}

} // namespace zui::debugger
