/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/debugger_view.h"
#include <algorithm>
#include <cstdio>
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
#include "debugger/project_state.h"
#include "frontend/sdl-fltk/bridge/symbols.h"
#include "debugger/registers_view.h"
#include "debugger/stack_view.h"
#include "debugger/toolbar.h"
#include "debugger/watch_panel.h"
#include "debugger/watch_breaks.h"
#include "tools/tool_registry.h"
#include "tools/tool_windows.h"
#include "theme/theme.h"
#include "widgets/dock_layout.h"
#include "widgets/split_pane.h"
#include "widgets/tabbed_panel.h"
#include "widgets/panel_stack.h"
#include "symbols/map_files.h"
#include "video/video_windows.h"

namespace zui::debugger {

using namespace zui::symbols;
using namespace zui::video;
using namespace zui::app;
using namespace zui::common;
using namespace zui::theme;
using namespace zui::tools;
using namespace zui::widgets;

namespace {

DebuggerView* s_instance;

/* Smallest sizes the split panes keep (see min_body_size) */
constexpr int MIN_SCREEN_H = 120;
constexpr int MIN_MEMORY_H = 140;
constexpr int MIN_REST_W = 520;

constexpr int DEFAULT_LEFT_W = FZ_SCREEN_WIDTH;
constexpr int DEFAULT_SCREEN_H = FZ_SCREEN_HEIGHT;
constexpr int DEFAULT_RIGHT_W = 300;
constexpr int DEFAULT_STACK_H = 120;
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
    project_state_init();

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
    /* Right column: CPU (its own height), Stack, Watch/Breakpoints tabs */
    m_side = new PanelStack(0, 0, DEFAULT_RIGHT_W, 800);
    m_regs = new RegistersView(0, 0, DEFAULT_RIGHT_W, 300);
    m_regs->end();
    m_stack = new StackView(0, 0, DEFAULT_RIGHT_W, DEFAULT_STACK_H);
    m_stack->end();
    m_lists = new TabbedPanel(0, 0, DEFAULT_RIGHT_W, 300);
    m_watch = new WatchPanel(0, 0, DEFAULT_RIGHT_W, 300);
    m_watch->end();
    m_breakpoints = new BreakpointsPanel(0, 0, DEFAULT_RIGHT_W, 300);
    m_breakpoints->end();
    m_lists->end();
    m_side->end();
    dock->end();

    m_lists->add_tab(m_watch);
    m_lists->add_tab(m_breakpoints);
    m_lists->on_select = [this](int) { capture_panels(); };
    m_side->add_panel(m_regs, 0);
    m_side->add_panel(m_stack, DEFAULT_STACK_H);
    m_side->add_panel(m_lists, 300, true);
    m_side->on_change = [this] { capture_panels(); };
    m_regs->on_layout = [this] {
        m_side->layout();
        capture_panels();
    };
    m_memory->collapsible(true);
    m_memory->on_collapse = [this](Panel*) { set_memory_collapsed(!m_memory->collapsed()); };

    m_left->set(screen, m_memory, DEFAULT_SCREEN_H, MIN_SCREEN_H, MIN_MEMORY_H);
    m_rest->set(m_disasm, m_side, DEFAULT_RIGHT_W, 260, 240);
    m_root->set(m_left, m_rest, default_left_w(), MemoryPanel::preferred_width(), MIN_REST_W);

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
    project_state_close();
    fz_map_forget();
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
    char reason[64];
    const bool watch_stop = paused && watch_hit_text(reason, sizeof(reason), false);
    m_toolbar->update(paused, current_regs().pc, watch_stop ? reason : nullptr);
    m_regs->refresh(paused, force);
    m_stack->refresh(paused, force);
    m_lists->refresh(paused, force);
    m_disasm->refresh(paused, force);
    m_memory->refresh(paused, force);
    dbg_t* dbg = fz_debugger();
    if (paused && dbg->breakpoint_stopped && dbg->breakpoint_hits != m_breakpoint_hits) {
        m_breakpoint_hits = dbg->breakpoint_hits;
        m_disasm->reveal_address(dbg->breakpoint_hit_addr);
        if (m_lists->collapsed()) m_side->toggle_collapse(m_lists);
        m_lists->select(1);
        m_breakpoints->reveal(dbg->breakpoint_hit_addr);
    }
    watchpoint_hit_t hit;
    if (watch_stop && debugger_get_watchpoint_hit(fz_debugger(), &hit) && hit.count != m_watch_hits) {
        /* A watchpoint stopped the machine: say why and show the byte */
        m_watch_hits = hit.count;
        notif_show("%s", reason);
        m_memory->go(hit.addr);
        /* Show the watch whose break fired */
        if (m_lists->collapsed()) m_side->toggle_collapse(m_lists);
        m_lists->select(0);
    }
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

bool DebuggerView::pause_reason(char* text, int size) const
{
    if (fz_paused() && fz_debugger()->breakpoint_stopped) {
        snprintf(text, size, "Breakpoint");
        return true;
    }
    return fz_paused() && watch_hit_text(text, (size_t) size, true);
}

void DebuggerView::open_map(bool append)
{
    map_open_dialog(append);
}

void DebuggerView::reload_maps()
{
    map_reload();
}

void DebuggerView::machine_bound()
{
    project_state_restore();
}

void DebuggerView::default_body_size(int& w, int& h) const
{
    w = default_left_w() + DEFAULT_DISASM_W + DEFAULT_RIGHT_W + 2 * SplitPane::GAP;
    h = DEFAULT_SCREEN_H + DEFAULT_MEMORY_H + SplitPane::GAP;
}

void DebuggerView::min_body_size(int& w, int& h) const
{
    /* The minimums given to the split panes in the constructor */
    w = MemoryPanel::preferred_width() + MIN_REST_W + SplitPane::GAP;
    h = MIN_SCREEN_H + MIN_MEMORY_H + SplitPane::GAP;
}

void DebuggerView::apply_layout()
{
    const LayoutConfig c = g_layout; /* copy: the setters below capture into g_layout */
    const int left_w = c.left_w > 0 ? c.left_w : default_left_w();
    m_root->fixed_size(left_w);
    m_left->fixed_size(c.screen_h > 0 ? c.screen_h : default_screen_h(left_w));
    m_rest->fixed_size(c.right_w > 0 ? c.right_w : DEFAULT_RIGHT_W);
    m_regs->set_more(c.cpu_more);
    m_regs->set_collapsed(c.cpu_collapsed);
    m_stack->set_collapsed(c.stack_collapsed);
    m_lists->set_collapsed(c.lists_collapsed);
    m_lists->select(c.lists_tab);
    m_side->set_size(1, c.stack_h > 0 ? c.stack_h : DEFAULT_STACK_H);
    set_memory_collapsed(c.memory_collapsed);
    g_layout = c;
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
    capture_panels();
    video_window_capture_config();
}

void DebuggerView::set_memory_collapsed(bool collapsed)
{
    m_memory->set_collapsed(collapsed);
    m_left->squeeze_second(collapsed ? HEADER_H : 0);
    g_layout.memory_collapsed = collapsed;
}

void DebuggerView::capture_panels()
{
    g_layout.stack_h = m_side->size_of(1);
    g_layout.cpu_more = m_regs->more();
    g_layout.cpu_collapsed = m_regs->collapsed();
    g_layout.stack_collapsed = m_stack->collapsed();
    g_layout.lists_collapsed = m_lists->collapsed();
    g_layout.lists_tab = m_lists->current();
    g_layout.memory_collapsed = m_memory->collapsed();
}

void DebuggerView::reset_layout()
{
    m_side->clear_maximize();
    g_layout = LayoutConfig();
    video_window_show(false);
    tool_windows_reset();
    apply_layout();
}

void DebuggerView::show_tool(app::ToolId tool, bool show)
{
    if (show && app::main_window()) app::main_window()->set_debug_layout(true);
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
    return new debugger::DebuggerView(dock, screen);
}

} // namespace zui::debugger
