/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "console/console_format.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <FL/fl_utf8.h>
#include "common/cp437.h"
#include "common/machine.h"
#include "frontend/sdl-fltk/bridge/console.h"

namespace zui::console {

using namespace zui::common;

void append_char(std::string& out, uint8_t b)
{
    if (b >= 0x20 && b < 0x7F) {
        out += (char) b;
        return;
    }
    char utf8[8];
    const unsigned cp = cp437_to_unicode(b) ? cp437_to_unicode(b) : ' ';
    const int n = fl_utf8encode(cp, utf8);
    out.append(utf8, (size_t) n);
}

void format_time(char* out, size_t size, uint64_t ns)
{
    snprintf(out, size, "%10.6f", (double) ns / 1e9);
}

std::string line_text(const fz_console_line_t& line, bool time)
{
    std::string s;
    if (time) {
        char t[32];
        format_time(t, sizeof(t), line.time_ns);
        s += t;
        s += "  ";
    }
    for (int i = 0; i < line.len; i++) append_char(s, (uint8_t) line.text[i]);
    return s;
}

std::string all_text(bool time)
{
    std::string s;
    const uint64_t first = fz_console_first();
    const int count = fz_console_count();
    for (int i = 0; i < count; i++) {
        fz_console_line_t line;
        if (!fz_console_line(first + (uint64_t) i, &line)) continue;
        s += line_text(line, time);
        s += '\n';
    }
    return s;
}

} // namespace zui::console
