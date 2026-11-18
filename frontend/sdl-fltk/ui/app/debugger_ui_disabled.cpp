/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Built instead of the debugger UI when the debugger is compiled out. */

#include "app/debugger_ui.h"
#include "app/screen_view.h"
#include "widgets/dock_layout.h"

namespace zui::app {

using namespace zui::widgets;

DebuggerUi* create_debugger_ui(widgets::DockLayout*, ScreenView*)
{
    return nullptr;
}

} // namespace zui::app
