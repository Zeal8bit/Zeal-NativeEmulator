/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Number formatting honouring the "Uppercase Hex" preference, and metrics
 * of the data font. */
#pragma once

#include <cstddef>

namespace zui::common {

const char* hex8_fmt();
const char* hex16_fmt();
void hex16(char* out, size_t size, unsigned value);
void hex8(char* out, size_t size, unsigned value);
/** Width of one character of the data font (set fl_font itself). */
int  char_width();
/** Baseline offset of data-font text inside a line of `line_h` pixels. */
int  text_baseline(int line_h);

} // namespace zui::common
