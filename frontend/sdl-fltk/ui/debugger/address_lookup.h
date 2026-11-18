/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* What the user types in an address field: hex or a symbol name. */
#pragma once

#include <string>
#include "common/machine.h"
#include <FL/Fl_Widget.H>

class Fl_Widget;

namespace zui::debugger {

/** Parse a hex address ("1234", "$1234", "0x1234") or an exact symbol. */
bool parse_address(const char* text, hwaddr* out);

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

} // namespace zui::debugger
