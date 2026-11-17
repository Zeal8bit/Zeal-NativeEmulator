/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Helpers shared by the debugger panels and tool views (see panel.h). */

#include <cstdio>
#include <cstring>
#include <vector>
#include <FL/fl_draw.H>
#include "panel.h"

namespace zui {

void hex16(char* out, size_t size, unsigned value)
{
    snprintf(out, size, hex16_fmt(), value & 0xFFFF);
}

void hex8(char* out, size_t size, unsigned value)
{
    snprintf(out, size, hex8_fmt(), value & 0xFF);
}

int char_width()
{
    fl_font(DATA_FONT, DATA_SIZE);
    return (int) fl_width("0");
}

int text_baseline(int line_h)
{
    fl_font(DATA_FONT, DATA_SIZE);
    return line_h - fl_descent() - (line_h - fl_height()) / 2;
}

void style_input(Fl_Input* in)
{
    in->textfont(DATA_FONT);
    in->textsize(DATA_SIZE);
    in->color(COLOR_BG);
    in->textcolor(COLOR_TEXT);
    in->cursor_color(COLOR_AMBER);
    in->selection_color(COLOR_SELECT);
    in->box(FL_BORDER_BOX);
}

void style_button(Fl_Button* b)
{
    b->labelsize(12);
    b->box(FL_FLAT_BOX);
    b->down_box(FL_FLAT_BOX);
    b->color(COLOR_RAISED);
    b->selection_color(COLOR_BORDER);
    b->labelcolor(COLOR_TEXT);
    b->clear_visible_focus();
}

void style_check(Fl_Check_Button* b)
{
    b->labelsize(12);
    b->labelcolor(COLOR_TEXT);
    b->selection_color(COLOR_AMBER);
    b->clear_visible_focus();
}

bool dbg_ready()
{
    return fz_debugger() != nullptr;
}

regs_t current_regs()
{
    regs_t regs;
    memset(&regs, 0, sizeof(regs));
#if CONFIG_ENABLE_DEBUGGER
    if (!dbg_ready()) return regs;
    if (!fz_paused() && fz_live_sample(&regs, nullptr, 0, nullptr, 0)) return regs;
    debugger_get_registers(fz_debugger(), &regs);
#endif
    return regs;
}

regs_t exact_regs()
{
    regs_t regs;
    memset(&regs, 0, sizeof(regs));
#if CONFIG_ENABLE_DEBUGGER
    if (dbg_ready()) debugger_get_registers(fz_debugger(), &regs);
#endif
    return regs;
}

void display_memory(hwaddr addr, int len, uint8_t* out, bool stack)
{
    if (!fz_paused()) {
        const bool ok = stack ? fz_live_sample(nullptr, out, len, nullptr, 0)
                              : fz_live_sample(nullptr, nullptr, 0, out, len);
        if (ok) return;
    }
#if CONFIG_ENABLE_DEBUGGER
    if (dbg_ready()) debugger_read_memory(fz_debugger(), addr, len, out);
#else
    (void) addr;
    memset(out, 0, (size_t) len);
#endif
}

/* ------------------------------------------------------------------ */
/*  Persistence                                                        */
/* ------------------------------------------------------------------ */

static std::vector<std::function<void(rini_config*)>>& persist_list()
{
    static std::vector<std::function<void(rini_config*)>> list;
    return list;
}

static void persist_save_all(rini_config* ini)
{
    for (auto& save : persist_list()) save(ini);
}

void persist_register(std::function<void(rini_config*)> save)
{
    persist_list().push_back(std::move(save));
    config_save_hook = persist_save_all;
}

/* ------------------------------------------------------------------ */
/*  Panel                                                              */
/* ------------------------------------------------------------------ */

void Panel::extra(const char* text)
{
    if (strcmp(m_extra, text) == 0) return;
    snprintf(m_extra, sizeof(m_extra), "%s", text);
    damage(FL_DAMAGE_ALL, x(), y(), w(), HEADER_H);
}

void Panel::draw()
{
    Fl_Group::draw();
    draw_header(x(), y(), w(), m_title, m_extra);
}

} // namespace zui
