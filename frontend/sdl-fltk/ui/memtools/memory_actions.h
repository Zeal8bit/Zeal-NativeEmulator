/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Dialogs behind the memory view's context menu: fill, save a range to a
 * file, load a file into memory, go to an address. */
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace zui::memtools {

/** Write bytes to memory through the memory view (keeps its snapshot). */
using Writer = std::function<bool(unsigned addr, const std::vector<uint8_t>& data)>;

/** Fill dialog: start/end prefilled, value byte or pattern; writes on OK. */
void fill_dialog(unsigned start, unsigned end, const Writer& write);
/** Save dialog: start/end prefilled, then a native file chooser. */
void save_range_dialog(unsigned start, unsigned end);
/** File chooser, then a confirmation with the destination (warns at $FFFF). */
void load_file_dialog(unsigned addr, const Writer& write);
/** Ask for an address or symbol; returns false on cancel. */
bool address_dialog(const char* title, unsigned initial, unsigned* out);

} // namespace zui::memtools
