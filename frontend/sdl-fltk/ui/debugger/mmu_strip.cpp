/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/mmu_strip.h"
#include <cstdio>
#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "theme/theme.h"

namespace zui::debugger {

using namespace zui::common;
using namespace zui::theme;

MmuStrip::MmuStrip(int X, int Y, int W, int H)
    : Fl_Widget(X, Y, W, H)
{
    tooltip("MMU: physical address and device of each 16 KB page. Click a page to view it.");
}

void MmuStrip::refresh()
{
    m_valid = dbg_ready() && debugger_custom(fz_debugger(), ZEAL_DBG_OP_GET_MMU, &m_info);
    redraw();
}

void MmuStrip::draw()
{
    fl_color(COLOR_SURFACE);
    fl_rectf(x(), y(), w(), h());
    if (!m_valid) {
        return;
    }
    const int seg_w = (w() - 8) / ZEAL_DBG_MMU_PAGES;
    char text[64], v[8];
    for (int i = 0; i < ZEAL_DBG_MMU_PAGES; i++) {
        const dbg_mmu_entry_t* e = &m_info.entries[i];
        const int sx = x() + 4 + i * seg_w;
        fl_color(COLOR_RAISED);
        fl_rectf(sx, y() + 3, seg_w - 3, h() - 6);
        fl_color(device_color(e->device));
        fl_rectf(sx, y() + 3, 2, h() - 6);
        hex16(v, sizeof(v), e->virt_addr);
        snprintf(text, sizeof(text), config.debugger.hex_upper ? "%s  %05X  %s" : "%s  %05x  %s",
                 v, (unsigned) e->phys_addr, e->device ? e->device : "--");
        fl_font(FL_HELVETICA, 11);
        fl_color(e->device ? COLOR_TEXT : COLOR_DIM);
        fl_draw(text, sx + 7, y() + 3, seg_w - 12, h() - 6, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
    }
}

int MmuStrip::handle(int event)
{
    if (event == FL_MOUSEWHEEL) {
        return 1;
    }
    if (event == FL_PUSH && m_valid) {
        const int seg_w = (w() - 8) / ZEAL_DBG_MMU_PAGES;
        const int i = (Fl::event_x() - x() - 4) / seg_w;
        if (i >= 0 && i < ZEAL_DBG_MMU_PAGES) {
            goto_memory(m_info.entries[i].virt_addr);
        }
        return 1;
    }
    return Fl_Widget::handle(event);
}

Fl_Color MmuStrip::device_color(const char* name)
{
    if (name == nullptr) {
        return COLOR_BORDER;
    }
    unsigned hsh = 0;
    for (const char* p = name; *p; p++) hsh = hsh * 31 + (unsigned char) *p;
    static const Fl_Color palette[] = { COLOR_AMBER, COLOR_CYAN, COLOR_GREEN, COLOR_CHANGED };
    return palette[hsh % 4];
}

} // namespace zui::debugger
