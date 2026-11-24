/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "trace/trace_view.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <FL/Fl_Button.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/machine_state.h"
#include "frontend/sdl-fltk/bridge/trace.h"
#include "theme/style.h"
#include "theme/theme.h"
#include "tools/tool_registry.h"
#include "trace/trace_list.h"
#include "trace/trace_text.h"
#include "widgets/icon_button.h"
#include "widgets/panel.h"

namespace zui::trace {

using namespace zui::common;
using namespace zui::debugger;
using namespace zui::theme;
using namespace zui::widgets;

static widgets::Panel* make_trace(int x, int y, int w, int h)
{
    return new TraceView(x, y, w, h);
}

static tools::ToolViewRegistrar s_reg(tools::TOOL_CODE, 30, "Trace", make_trace);

TraceView::TraceView(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "TRACE")
{
    m_record = new IconButton(X, Y, 10, 10, Icon::Breakpoint, "Record");
    m_record->callback(cb_record, this);
    m_record->tooltip("Record executed instructions while the debugger is on\n"
                      "(costs about 10% of emulation speed; never runs with the debugger off)");
    m_clear = new Fl_Button(X, Y, 10, 10, "Clear");
    style_button(m_clear);
    m_clear->callback(cb_clear, this);
    m_clear->tooltip("Forget the recorded instructions");
    m_save = new Fl_Button(X, Y, 10, 10, "Save...");
    style_button(m_save);
    m_save->callback(cb_save, this);
    m_save->tooltip("Save the whole trace to a text file, oldest first");
    m_bar = new Fl_Scrollbar(X, Y, 10, 10);
    m_bar->type(FL_VERTICAL);
    m_bar->linesize(1);
    m_bar->color(COLOR_SURFACE);
    m_bar->callback(cb_bar, this);
    m_list = new TraceList(X, Y, 10, 10, m_bar);
    m_list->tooltip("Newest first; registers are before each instruction (pink: changed by it)\n"
                    "Click: show in disassembly    Right-click: more");
    resizable(m_list);
    end();
    resize(X, Y, W, H);
    if (fz_trace_capacity() == 0) {
        m_record->deactivate();
        m_clear->deactivate();
        m_save->deactivate();
    }
    update_controls();
    snapshot();
}

void TraceView::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    const int by = Y + HEADER_H + 4;
    const int bh = BAR_H - 8;
    m_record->resize(X + 6, by, 84, bh);
    m_clear->resize(X + 96, by, 60, bh);
    m_save->resize(X + 162, by, 66, bh);
    const int ly = Y + HEADER_H + BAR_H;
    m_bar->resize(X + W - 14, ly, 14, H - (ly - Y));
    m_list->resize(X, ly, W - 14, H - (ly - Y));
}

void TraceView::refresh(bool paused, bool force)
{
    (void) paused;
    update_controls();
    /* ~4 Hz while running: copy the ring only when something was recorded */
    if (force || fz_trace_serial() != m_serial) {
        snapshot();
    } else if (m_list->symbols().update()) {
        m_list->redraw();
    }
    update_status();
}

void TraceView::draw()
{
    Panel::draw();
    fl_font(FL_HELVETICA, 12);
    fl_color(fz_trace_enabled() ? COLOR_GREEN : COLOR_DIM);
    const int sx = m_save->x() + m_save->w() + 12;
    fl_draw(m_status, sx, y() + HEADER_H, x() + w() - 10 - sx, BAR_H, FL_ALIGN_RIGHT | FL_ALIGN_CLIP);
}

void TraceView::cb_record(Fl_Widget*, void* data)
{
    TraceView* self = static_cast<TraceView*>(data);
    fz_trace_set_enabled(!fz_trace_enabled());
    self->update_controls();
    self->update_status();
}

void TraceView::cb_clear(Fl_Widget*, void* data)
{
    TraceView* self = static_cast<TraceView*>(data);
    fz_trace_clear();
    self->snapshot();
    self->update_status();
}

void TraceView::cb_save(Fl_Widget*, void* data)
{
    TraceView* self = static_cast<TraceView*>(data);
    self->snapshot();
    save_trace_dialog(self->m_list->entries(), self->m_list->symbols());
}

void TraceView::cb_bar(Fl_Widget*, void* data)
{
    TraceView* self = static_cast<TraceView*>(data);
    self->m_list->scroll_to_row(self->m_bar->value());
}

void TraceView::update_controls()
{
    const bool on = fz_trace_enabled();
    if ((int) on == m_shown_enabled) {
        return;
    }
    m_shown_enabled = (int) on;
    m_record->icon(on ? Icon::Pause : Icon::Breakpoint);
    m_record->text(on ? "Stop" : "Record");
    m_record->icon_color(on ? COLOR_DIM : COLOR_BREAK);
    extra(fz_trace_capacity() == 0 ? "COMPILED OUT" : on ? "RECORDING" : "STOPPED");
    set_empty_text();
    redraw();
}

void TraceView::update_status()
{
    char count[32];
    char cap[32];
    with_commas(count, sizeof(count), m_list->entries().size());
    with_commas(cap, sizeof(cap), fz_trace_capacity());
    char text[sizeof(count) + sizeof(cap) + 16];
    snprintf(text, sizeof(text), "%s / %s instructions", count, cap);
    if (strcmp(text, m_status) != 0) {
        snprintf(m_status, sizeof(m_status), "%s", text);
        damage(FL_DAMAGE_ALL, x(), y() + HEADER_H, w(), BAR_H);
    }
}

void TraceView::snapshot()
{
    const unsigned cap = fz_trace_capacity();
    std::vector<zeal_trace_entry_t> entries(cap);
    const uint64_t serial = fz_trace_serial();
    entries.resize(fz_trace_snapshot(entries.data(), cap));
    m_serial = serial;
    m_list->set_entries(std::move(entries), serial);
    set_empty_text();
}

void TraceView::set_empty_text()
{
    if (fz_trace_capacity() == 0) {
        m_list->empty_title = "Trace compiled out";
        m_list->empty_hint = "This build was configured with -Dtrace_depth=0. Rebuild with a "
                             "depth (default 4096) to record executed instructions and the call stack.";
    } else if (!dbg_ready()) {
        m_list->empty_title = "Trace unavailable";
        m_list->empty_hint = "No machine is running.";
    } else if (!fz_trace_enabled()) {
        m_list->empty_title = "Press Record to trace execution";
        m_list->empty_hint = "While recording, the debugger keeps the last instructions executed "
                             "with their registers, interrupts and resets: pause the machine (or hit a "
                             "breakpoint) to see how it got there.";
    } else {
        m_list->empty_title = "Recording... nothing executed yet";
        m_list->empty_hint = "Instructions are recorded while the machine runs with the debugger on. "
                             "Newest first; click a line to show it in the disassembly.";
    }
}

} // namespace zui::trace
