/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Building blocks shared by every debugger panel and tool view:
 * the Panel base class, styling, formatting, register access and
 * cross-panel navigation. New panels/views include this header only. */
#pragma once

#include <functional>
#include <string>
#include <FL/Fl_Input.H>
#include <FL/Fl_Check_Button.H>
#include "ui.h"
#include "debugger/address_lookup.h"
#include "debugger/machine_state.h"
#include "debugger/navigation.h"
#include "widgets/inline_edit.h"
#include "common/cp437.h"

namespace zui {
using namespace zui::debugger;
}

namespace zui {

/* ------------------------------------------------------------------ */
/*  Panels living in the main debugger window                          */
/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */
/*  Map files (view_symbols.cpp)                                       */
/* ------------------------------------------------------------------ */

/** Ask for a map file and load it: replaces the symbols, or adds to them
 *  when `append`. Returns true if a file was loaded. */
bool map_open_dialog(bool append);
/** Reload the loaded map files (after rebuilding the program). */
void map_reload();
/** Remove every symbol. */
void map_clear();

} // namespace zui
