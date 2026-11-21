/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Text of captured UART lines: CP437 bytes as UTF-8, optional timestamps. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include "frontend/sdl-fltk/bridge/console.h"
#include "common/machine.h"

namespace zui::console {

/** Append one received byte as UTF-8 (control bytes shown as symbols). */
void append_char(std::string& out, uint8_t b);
/** Emulated time as seconds with millisecond precision. */
void format_time(char* out, size_t size, uint64_t ns);
/** One line, optionally prefixed with its timestamp. */
std::string line_text(const fz_console_line_t& line, bool time);
/** Every kept line. */
std::string all_text(bool time);

} // namespace zui::console
