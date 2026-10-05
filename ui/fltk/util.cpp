// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/util.h"
#include "debugger/frontend.h"
#include "ui/fltk/cp437.h"
#include "ui/fltk/debugger_ui.h"
#include <FL/fl_draw.H>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace zeal_ui
{
double now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

uint32_t address(dbg_ui_t *ui, const char *s, bool *valid)
{
    hwaddr a = 0;
    bool ok = debugger_find_symbol(ui->host.debugger, s, &a);
    if (!ok) {
        if (*s == '$')
            ++s;
        char *end = nullptr;
        unsigned long n = std::strtoul(s, &end, 16);
        ok = *s && end && !*end && n <= 0xffff;
        a = (uint32_t)n;
    }
    if (valid)
        *valid = ok;
    return a;
}

void text(dbg_ui_t *u, const char *role, const std::string &s, int x, int y, bool mono)
{
    fl_color(u->color(role));
    fl_font(mono ? u->mono_font : u->ui_font, mono ? u->theme.mono_size : u->theme.font_size);
    fl_draw(s.c_str(), x, y);
}

std::string hex(unsigned n, int digits, bool upper)
{
    char b[32];
    std::snprintf(b, sizeof(b), upper ? "%0*X" : "%0*x", digits, n);
    return b;
}

int hex_digit(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

int mono_chars(dbg_ui_t *u, int count)
{
    fl_font(u->mono_font, u->theme.mono_size);
    return count * (int(fl_width("0")) + 1) + 2 * u->theme.spacing;
}

std::string glyph(uint8_t b, bool cp)
{
    if (!cp)
        return std::string(1, b >= 32 && b < 127 ? char(b) : '.');
    char out[8] = {};
    int n = fl_utf8encode(s_cp437_to_unicode[b], out);
    return std::string(out, n);
}
} // namespace zeal_ui
