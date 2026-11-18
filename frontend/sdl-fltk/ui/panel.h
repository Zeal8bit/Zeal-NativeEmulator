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

namespace zui {

/* ------------------------------------------------------------------ */
/*  Machine access helpers (common.cpp)                                */
/* ------------------------------------------------------------------ */

/** True once the debugger is available (machine bound, debugger built). */
bool   dbg_ready();
/** Registers to display: exact when paused, the mid-frame sample while
 *  running (reading them between frames always lands on the frame boundary). */
regs_t current_regs();
/** Registers for editing: always the exact current state. */
regs_t exact_regs();
/** Memory at SP (stack=true) or HL as displayed: sampled while running. */
void   display_memory(hwaddr addr, int len, uint8_t* out, bool stack);

/* ------------------------------------------------------------------ */
/*  Navigation and refresh (implemented by the debugger)               */
/* ------------------------------------------------------------------ */

/** Refresh every debugger panel and tool view now (after a user action). */
void request_refresh();
/**
 * Resolve what the user typed in an address field: hex ($C000, 0xC000,
 * C000) or a symbol. Symbols are looked up loosely (case, leading
 * underscores, prefix, substring); several matches pop up a menu below
 * `anchor` to pick from, listing `prefer_kind` (symbol_kind_t, or -1)
 * first. `picked`, if given, receives the chosen symbol's name (empty for
 * hex). Returns false if nothing matched or the menu was dismissed.
 */
bool lookup_address(Fl_Widget* anchor, const char* text, hwaddr* out, int prefer_kind = -1,
                    std::string* picked = nullptr);

/** Show an address in the main window's memory editor / disassembly. */
void goto_memory(hwaddr addr);
void goto_disassembly(hwaddr addr);

/* ------------------------------------------------------------------ */
/*  Panels living in the main debugger window                          */
/* ------------------------------------------------------------------ */

/** Watch panel (view_watch.cpp), right column between Stack and Breakpoints. */
Panel* create_watch_panel(int x, int y, int w, int h);

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
