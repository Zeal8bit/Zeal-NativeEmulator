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


} // namespace zui
