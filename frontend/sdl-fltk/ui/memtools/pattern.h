/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Byte-pattern search of the memory editor. Plain C++ with no FLTK or
 * machine dependency, so it can be unit-tested on its own. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zui::memtools {

/** Byte pattern: mask[i] == 0 marks a wildcard byte. */
struct Pattern {
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> mask;
    int size() const { return (int) bytes.size(); }
    bool empty() const { return bytes.empty(); }
    bool matches(const uint8_t* at) const;
};

/**
 * Parse a search or fill pattern. Tokens are separated by spaces or commas:
 * hex bytes (`3E`, `$3E`, `0x3E`, or runs like `3E2200`), `??` wildcards
 * (only when `allow_wildcards`) and quoted strings (`"HELLO"`, `\"` and `\\`
 * escapes). Returns false with a short message in `error` on bad input.
 */
bool parse_pattern(const char* text, Pattern& out, std::string* error, bool allow_wildcards = true);

/**
 * Find `p` in `mem[0..size)`, starting at `from` (inclusive) and moving
 * forward or backward, wrapping around once. A match never runs past the
 * end of the buffer. Returns the match address or -1; `wrapped` tells
 * whether the search went around the end (or start).
 */
int find(const uint8_t* mem, int size, const Pattern& p, int from, bool forward, bool* wrapped = nullptr);

/** Byte value: hex (`3E`, `$3E`, `0x3E`) or decimal with `#` (`#62`). */
bool parse_byte(const char* text, uint8_t* out);

} // namespace zui::memtools
