/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/debugger_view.h"
#include <algorithm>
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include "app/debugger_ui.h"
#include "app/main_window.h"
#include "app/screen_view.h"
#include "common/clock.h"
#include "common/machine.h"
#include "debugger/breakpoints_panel.h"
#include "debugger/disasm_panel.h"
#include "debugger/layout_config.h"
#include "debugger/machine_state.h"
#include "debugger/memory_panel.h"
#include "debugger/registers_view.h"
#include "debugger/stack_view.h"
#include "debugger/toolbar.h"
#include "debugger/watch_panel.h"
#include "panel.h"
#include "tools/tool_registry.h"
#include "tools/tool_windows.h"
#include "video.h"
#include "widgets/dock_layout.h"
#include "widgets/split_pane.h"

namespace zui::debugger {

using namespace zui::app;
using namespace zui::common;
using namespace zui::tools;
using namespace zui::widgets;

namespace {

DebuggerView* s_instance;

constexpr int DEFAULT_LEFT_W = FZ_SCREEN_WIDTH;
constexpr int DEFAULT_SCREEN_H = FZ_SCREEN_HEIGHT;
constexpr int DEFAULT_RIGHT_W = 300;
constexpr int DEFAULT_BREAKPOINTS_H = 150;
constexpr int DEFAULT_WATCH_H = 170;
constexpr int DEFAULT_DISASM_W = 420;
constexpr int DEFAULT_MEMORY_H = 300;


int default_left_w()
{
    const int pref = MemoryPanel::preferred_width();
    return pref > DEFAULT_LEFT_W ? pref : DEFAULT_LEFT_W;
}

/* The screen fills the column width at 4:3; memory gets the rest */
int default_screen_h(int left_w)
{
    const int body_h = DEFAULT_SCREEN_H + DEFAULT_MEMORY_H;
    const int fill = left_w * FZ_SCREEN_HEIGHT / FZ_SCREEN_WIDTH;
    const int min_memory = 160;
    return std::max(DEFAULT_SCREEN_H, std::min(fill, body_h - min_memory));
}

} // namespace

DebuggerView* DebuggerView::instance()
{
    return s_instance;
}

DebuggerView::DebuggerView(DockLayout* dock, ScreenView* screen) : m_dock(dock)
{
    s_instance = this;
    layout_config_load();

    dock->begin();
    m_root = new SplitPane(0, 0, 1400, 800, true, false);
    m_root->end();
    m_left = new SplitPane(0, 0, default_left_w(), 800, false, false);
    m_left->end();
    m_memory = new MemoryPanel(0, 0, default_left_w(), DEFAULT_MEMORY_H);
    m_memory->end();
    m_rest = new SplitPane(0, 0, 700, 800, true, true);
    m_rest->end();
    m_disasm = new DisasmPanel(0, 0, DEFAULT_DISASM_W, 800);
    m_disasm->end();
    m_right = new SplitPane(0, 0, DEFAULT_RIGHT_W, 800, false, false);
    m_right->end();
    m_regs = new RegistersView(0, 0, DEFAULT_RIGHT_W, RegistersView::preferred_height());
    m_regs->end();
    m_lower = new SplitPane(0, 0, DEFAULT_RIGHT_W, 400, false, true);
    m_lower->end();
    m_stack = new StackView(0, 0, DEFAULT_RIGHT_W, 200);
    m_stack->end();
    m_tail = new SplitPane(0, 0, DEFAULT_RIGHT_W, 300, false, true);
    m_tail->end();
    m_watch = new WatchPanel(0, 0, DEFAULT_RIGHT_W, DEFAULT_WATCH_H);
    m_watch->end();
    m_breakpoints = new BreakpointsPanel(0, 0, DEFAULT_RIGHT_W, DEFAULT_BREAKPOINTS_H);
    m_breakpoints->end();
    dock->end();

    m_left->set(screen, m_memory, DEFAULT_SCREEN_H, 120, 140);
    m_tail->set(m_watch, m_breakpoints, DEFAULT_BREAKPOINTS_H, 60, 90);
    m_lower->set(m_stack, m_tail, DEFAULT_WATCH_H + DEFAULT_BREAKPOINTS_H + SplitPane::GAP, 60, 160);
    m_right->set(m_regs, m_lower, RegistersView::preferred_height(), 120, 150);
    m_rest->set(m_disasm, m_right, DEFAULT_RIGHT_W, 260, 240);
    m_root->set(m_left, m_rest, default_left_w(), MemoryPanel::preferred_width(), 520);

    dock->set_root(m_root, [this](bool expanded) {
        m_root->collapse(!expanded);
        m_left->collapse(!expanded);
        if (!expanded) {
            video_window_suspend();
            tool_windows_suspend();
        }
    });
}

DebuggerView::~DebuggerView()
{
    video_window_destroy();
    tool_windows_destroy();
    s_instance = nullptr;
}

Fl_Group* DebuggerView::toolbar(int x, int y, int w, int h)
{
    m_toolbar = new Toolbar(x, y, w, h);
    return m_toolbar;
}

void DebuggerView::refresh(bool force)
{
    if (!dbg_ready() || m_toolbar == nullptr) return;
    const bool paused = fz_paused();
    const double now = now_seconds();
    if (!force && (paused || now - m_last_refresh < 0.1)) return;
    m_last_refresh = now;
    m_toolbar->update(paused, current_regs().pc);
    m_regs->refresh(paused, force);
    m_stack->refresh(paused, force);
    m_watch->refresh(paused, force);
    m_breakpoints->refresh(paused, force);
    m_disasm->refresh(paused, force);
    m_memory->refresh(paused, force);
    video_window_refresh(paused, force);
    tool_windows_refresh(paused, force);
}

void DebuggerView::command(app::DebugCommand command)
{
    dbg_t* dbg = fz_debugger();
    if (dbg == nullptr) return;
    switch (command) {
        case app::DebugCommand::Continue: debugger_continue(dbg); break;
        case app::DebugCommand::Pause: debugger_pause(dbg); break;
        case app::DebugCommand::Step: debugger_step(dbg); break;
        case app::DebugCommand::StepOver: debugger_step_over(dbg); break;
        case app::DebugCommand::ToggleBreakpointAtPc:
            debugger_breakpoint(dbg);
            refresh(true);
            break;
    }
}

bool DebuggerView::paused_at(unsigned* pc) const
{
    dbg_t* dbg = fz_debugger();
    if (!fz_paused() || dbg == nullptr) return false;
    regs_t regs;
    debugger_get_registers(dbg, &regs);
    if (pc) *pc = regs.pc;
    return true;
}

void DebuggerView::open_map(bool append)
{
    map_open_dialog(append);
}

void DebuggerView::reload_maps()
{
    map_reload();
}

void DebuggerView::default_body_size(int& w, int& h) const
{
    w = default_left_w() + DEFAULT_DISASM_W + DEFAULT_RIGHT_W + 2 * SplitPane::GAP;
    h = DEFAULT_SCREEN_H + DEFAULT_MEMORY_H + SplitPane::GAP;
}

void DebuggerView::apply_layout()
{
    const LayoutConfig& c = g_layout;
    const int bps_h = c.breakpoints_h > 0 ? c.breakpoints_h : DEFAULT_BREAKPOINTS_H;
    const int watch_h = c.watch_h > 0 ? c.watch_h : DEFAULT_WATCH_H;
    const int left_w = c.left_w > 0 ? c.left_w : default_left_w();
    m_root->fixed_size(left_w);
    m_left->fixed_size(c.screen_h > 0 ? c.screen_h : default_screen_h(left_w));
    m_rest->fixed_size(c.right_w > 0 ? c.right_w : DEFAULT_RIGHT_W);
    m_lower->fixed_size(watch_h + bps_h + SplitPane::GAP);
    m_tail->fixed_size(bps_h);
    if (m_dock->docks_visible()) {
        /* Open tool windows once the main window is on screen, so they float above it */
        Fl::add_timeout(0.3, [](void*) {
            if (main_window() && main_window()->shown() && main_window()->debug_layout()) {
                if (g_layout.video_shown) video_window_show(true);
                tool_windows_resume();
                main_window()->update_menus();
            }
        });
    }
}

void DebuggerView::capture_layout()
{
    if (!m_dock->docks_visible()) return;
    g_layout.left_w = m_root->fixed_size();
    g_layout.screen_h = m_left->fixed_size();
    g_layout.right_w = m_rest->fixed_size();
    g_layout.breakpoints_h = m_tail->fixed_size();
    g_layout.watch_h = m_lower->fixed_size() - m_tail->fixed_size() - SplitPane::GAP;
    video_window_capture_config();
}

void DebuggerView::reset_layout()
{
    g_layout = LayoutConfig();
    video_window_show(false);
    tool_windows_reset();
    apply_layout();
}

void DebuggerView::show_tool(app::ToolId tool, bool show)
{
    switch (tool) {
        case app::ToolId::Video:
            video_window_show(show);
            if (show) refresh(true);
            break;
        case app::ToolId::Code: tool_window_show(TOOL_CODE, show); break;
        case app::ToolId::Hardware: tool_window_show(TOOL_HARDWARE, show); break;
    }
}

bool DebuggerView::tool_shown(app::ToolId tool) const
{
    switch (tool) {
        case app::ToolId::Video: return video_window_shown();
        case app::ToolId::Code: return tool_window_shown(TOOL_CODE);
        case app::ToolId::Hardware: return tool_window_shown(TOOL_HARDWARE);
    }
    return false;
}


} // namespace zui::debugger

namespace zui::app {

DebuggerUi* create_debugger_ui(widgets::DockLayout* dock, ScreenView* screen)
{
    /* Keep Nuklear's panel settings in the saved configuration */
    for (int i = 0; i < FRONTEND_PANEL_COUNT; i++) {
        config_panel_load(&config_panels[i]);
    }
    return new debugger::DebuggerView(dock, screen);
}

} // namespace zui::debugger
