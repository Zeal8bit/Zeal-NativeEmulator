/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Private C++ declarations of the FLTK user interface. */
#pragma once

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Menu_Bar.H>
#include <cstdint>
#include <functional>

extern "C" {
#include "frontend/sdl-fltk/bridge/machine.h"
#include "debugger/debugger.h"
#include "utils/config.h"
#include "frontend/common/panels.h"
#include "utils/notif.h"
}

#include "theme/theme.h"
#include "theme/fonts.h"
#include "theme/style.h"
#include "common/clock.h"
#include "common/format.h"
#include "common/machine.h"
#include "common/persist.h"
#include "widgets/dock_layout.h"
#include "widgets/icon_button.h"
#include "widgets/panel.h"
#include "widgets/split_pane.h"
#include "app/debugger_ui.h"
#include "app/main_window.h"
#include "app/screen_view.h"
namespace zui {
using namespace zui::theme;
using namespace zui::common;
using namespace zui::widgets;
using namespace zui::app;
}

namespace zui {

/** Parse a hex address ("1234", "$1234", "0x1234") or a debugger symbol. */
bool parse_address(const char* text, hwaddr* out);

/* ------------------------------------------------------------------ */
/*  Debugger (debugger.cpp); stubs when the debugger is compiled out   */
/* ------------------------------------------------------------------ */

class Debugger : public app::DebuggerUi {
public:
    /** Builds the debugger layout around `screen` inside `dock`. */
    Debugger(DockLayout* dock, ScreenView* screen);
    ~Debugger() override;

    Fl_Group* toolbar(int x, int y, int w, int h) override;
    void refresh(bool force) override;
    void command(app::DebugCommand command) override;
    bool paused_at(unsigned* pc) const override;
    void default_body_size(int& w, int& h) const override;
    void apply_layout() override;
    void capture_layout() override;
    void reset_layout() override;
    void show_tool(app::ToolId tool, bool show) override;
    bool tool_shown(app::ToolId tool) const override;
    void open_map(bool append) override;
    void reload_maps() override;

    struct Impl;
private:
    Impl* m;
};

} // namespace zui
