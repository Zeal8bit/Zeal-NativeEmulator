/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/format.h"
#include <cstdio>
#include <FL/fl_draw.H>
#include "common/machine.h"
#include "theme/theme.h"

namespace zui::common {

using namespace zui::theme;

const char* hex8_fmt()
{
    return config.debugger.hex_upper ? "%02X" : "%02x";
}

const char* hex16_fmt()
{
    return config.debugger.hex_upper ? "%04X" : "%04x";
}

void hex16(char* out, size_t size, unsigned value)
{
    snprintf(out, size, hex16_fmt(), value & 0xFFFF);
}

void hex8(char* out, size_t size, unsigned value)
{
    snprintf(out, size, hex8_fmt(), value & 0xFF);
}

int char_width()
{
    fl_font(DATA_FONT, DATA_SIZE);
    return (int) fl_width("0");
}

int text_baseline(int line_h)
{
    fl_font(DATA_FONT, DATA_SIZE);
    return line_h - fl_descent() - (line_h - fl_height()) / 2;
}

} // namespace zui::common
