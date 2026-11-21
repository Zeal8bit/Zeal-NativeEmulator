/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Text form of trace entries: event names, a line per entry for the
 * clipboard, and saving the whole trace to a text file. */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "frontend/sdl-fltk/bridge/trace.h"
#include "trace/symbol_index.h"

namespace zui::trace {

/** "IRQ IM2", "NMI", "RESET", "UNTRACED"; nullptr for an instruction. */
const char* event_name(uint8_t kind);
/** Event description shown instead of the instruction: "IRQ IM2 -> $0038 handler". */
void event_text(char* out, size_t size, const zeal_trace_entry_t& e, const SymbolIndex& syms);
/** Bytes of a recorded instruction ("CD 00 02"), only as many as it is long. */
void bytes_text(char* out, size_t size, const zeal_trace_entry_t& e, int length);
/** Column titles matching trace_line(). */
std::string trace_header();
/** One entry as a line of text (no newline). */
std::string trace_line(const zeal_trace_entry_t& e, const SymbolIndex& syms);
/** Ask for a file name and write `newest_first` oldest first. */
void save_trace_dialog(const std::vector<zeal_trace_entry_t>& newest_first, const SymbolIndex& syms);

} // namespace zui::trace
