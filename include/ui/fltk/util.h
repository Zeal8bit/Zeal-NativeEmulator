// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <string>

struct dbg_ui_t;

namespace zeal_ui
{
double now();
// Accepts "$1234" or a symbol name, resolved through the debugger's symbol table.
uint32_t address(dbg_ui_t *ui, const char *s, bool *valid = nullptr);
// Draws a string in a themed role colour with the UI or monospace font.
void text(dbg_ui_t *u, const char *role, const std::string &s, int x, int y, bool mono = true);
std::string hex(unsigned n, int digits, bool upper = true);
int hex_digit(int c);
// Renders one byte either as printable ASCII or as its CP437 glyph.
std::string glyph(uint8_t b, bool cp);
// Width of `count` monospace characters plus a padding on each side; used to size
// table columns from their content instead of magic pixel counts.
int mono_chars(dbg_ui_t *u, int count);
} // namespace zeal_ui
