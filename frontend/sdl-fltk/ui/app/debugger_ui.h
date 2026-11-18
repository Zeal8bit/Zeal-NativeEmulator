/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* What the main window needs from the debugger UI. The debugger implements
 * it (debugger/debugger.cpp); without the debugger, Meson builds
 * app/debugger_ui_disabled.cpp instead and create_debugger_ui() returns
 * nullptr, so app/ never depends on debugger code. */
#pragma once

class Fl_Group;

namespace zui::widgets {
class DockLayout;
}

namespace zui::app {

class ScreenView;

/** Debug menu commands. */
enum class DebugCommand { Continue, Pause, Step, StepOver, ToggleBreakpointAtPc };

/** Floating windows of the debugger. */
enum class ToolId { Video, Code, Hardware };

class DebuggerUi {
public:
    virtual ~DebuggerUi() = default;

    /** Toolbar shown above the layout in debugger mode. */
    virtual Fl_Group* toolbar(int x, int y, int w, int h) = 0;
    /** Refresh the panels (throttled unless `force`). */
    virtual void refresh(bool force) = 0;
    virtual void command(DebugCommand command) = 0;
    /** True when paused; `pc` receives the program counter. */
    virtual bool paused_at(unsigned* pc) const = 0;

    /** Window body size that fits the default layout. */
    virtual void default_body_size(int& w, int& h) const = 0;
    /** Restore/save the split sizes, or go back to the defaults. */
    virtual void apply_layout() = 0;
    virtual void capture_layout() = 0;
    virtual void reset_layout() = 0;

    virtual void show_tool(ToolId tool, bool show) = 0;
    virtual bool tool_shown(ToolId tool) const = 0;

    /** Ask for a map file (replace, or add to the loaded symbols). */
    virtual void open_map(bool append) = 0;
    virtual void reload_maps() = 0;
};

/** Builds the debugger layout around `screen` inside `dock`; nullptr when
 *  the debugger is not compiled in. */
DebuggerUi* create_debugger_ui(widgets::DockLayout* dock, ScreenView* screen);

} // namespace zui::app
