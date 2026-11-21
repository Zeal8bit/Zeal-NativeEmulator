/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/format.h"
#include <cstdio>
#include <cstring>
#include <FL/fl_draw.H>
#include "common/machine.h"
#include "theme/theme.h"
#include <cstdint>

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

void with_commas(char* out, size_t size, uint64_t v)
{
    char raw[32];
    snprintf(raw, sizeof(raw), "%llu", (unsigned long long) v);
    const int len = (int) strlen(raw);
    size_t o = 0;
    for (int i = 0; i < len && o + 1 < size; i++) {
        if (i > 0 && (len - i) % 3 == 0 && o + 2 < size) out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = '\0';
}

} // namespace zui::common
