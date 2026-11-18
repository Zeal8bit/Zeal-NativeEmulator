/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Debugger layout. Everything needed while stepping stays visible together,
 * no tabs (in the spirit of Mesen and no$sns):
 *
 *   +---------------------------------------------------------------+
 *   | toolbar: run/pause  step  over  run-to | reset          | state  |
 *   +----------------+---------------------+-----------------------+
 *   | screen         | disassembly         | CPU registers, flags  |
 *   |                |                     +-----------------------+
 *   +----------------+                     | stack (SP+n)          |
 *   | memory + MMU   |                     +-----------------------+
 *   |                |                     | breakpoints           |
 *   +----------------+---------------------+-----------------------+
 *
 * Graphics viewers and the profiler live in a separate tool window
 * (video/). Split sizes are saved under FLTK_* config keys. */
#pragma once

#include <cstdint>
#include "app/debugger_ui.h"
#include "app/screen_view.h"
#include <FL/Fl_Group.H>

namespace zui::widgets {
class DockLayout;
class Panel;
class SplitPane;
}

namespace zui::debugger {

class BreakpointsPanel;
class DisasmPanel;
class MemoryPanel;
class RegistersView;
class StackView;
class Toolbar;

class DebuggerView : public app::DebuggerUi {
public:
    /** Builds the debugger layout around `screen` inside `dock`. */
    DebuggerView(widgets::DockLayout* dock, app::ScreenView* screen);
    ~DebuggerView() override;

    /** The debugger UI, nullptr until created. */
    static DebuggerView* instance();
    DisasmPanel* disasm() const { return m_disasm; }
    MemoryPanel* memory() const { return m_memory; }

    Fl_Group* toolbar(int x, int y, int w, int h) override;
    void refresh(bool force) override;
    void command(app::DebugCommand command) override;
    bool paused_at(unsigned* pc) const override;
    bool pause_reason(char* text, int size) const override;
    void default_body_size(int& w, int& h) const override;
    void apply_layout() override;
    void capture_layout() override;
    void reset_layout() override;
    void show_tool(app::ToolId tool, bool show) override;
    bool tool_shown(app::ToolId tool) const override;
    void open_map(bool append) override;
    void reload_maps() override;

private:
    widgets::DockLayout* m_dock;
    Toolbar* m_toolbar = nullptr;
    widgets::SplitPane* m_root = nullptr;   /* left | rest */
    widgets::SplitPane* m_left = nullptr;   /* screen / memory */
    widgets::SplitPane* m_rest = nullptr;   /* disassembly | right */
    widgets::SplitPane* m_right = nullptr;  /* registers / lower */
    widgets::SplitPane* m_lower = nullptr;  /* stack / tail */
    widgets::SplitPane* m_tail = nullptr;   /* watch / breakpoints */
    widgets::Panel* m_watch = nullptr;
    RegistersView* m_regs = nullptr;
    StackView* m_stack = nullptr;
    BreakpointsPanel* m_breakpoints = nullptr;
    DisasmPanel* m_disasm = nullptr;
    MemoryPanel* m_memory = nullptr;
    double m_last_refresh = 0;
    uint32_t m_watch_hits = 0; /* watchpoint hits already reported */
};

} // namespace zui::debugger
