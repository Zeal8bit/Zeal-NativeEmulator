/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/navigation.h"
#include "app/main_window.h"
#include "debugger/debugger_view.h"
#include "debugger/disasm_panel.h"
#include "debugger/memory_panel.h"
#include "common/machine.h"

namespace zui::debugger {

using namespace zui::app;
using namespace zui::widgets;

void request_refresh()
{
    if (DebuggerView* debugger = DebuggerView::instance()) debugger->refresh(true);
    if (app::MainWindow* window = app::main_window()) window->update_status();
}

void goto_memory(hwaddr addr)
{
    if (DebuggerView* debugger = DebuggerView::instance()) debugger->memory()->go(addr);
}

void goto_disassembly(hwaddr addr)
{
    if (DebuggerView* debugger = DebuggerView::instance()) debugger->disasm()->go(addr);
}

} // namespace zui::debugger
