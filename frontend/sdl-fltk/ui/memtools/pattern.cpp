/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "memtools/pattern.h"
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace zui::memtools {

namespace {

int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool fail(std::string* error, const char* msg)
{
    if (error) *error = msg;
    return false;
}

} // namespace

bool Pattern::matches(const uint8_t* at) const
{
    for (size_t i = 0; i < bytes.size(); i++) {
        if (mask[i] && at[i] != bytes[i]) return false;
    }
    return true;
}

bool parse_pattern(const char* text, Pattern& out, std::string* error, bool allow_wildcards)
{
    out.bytes.clear();
    out.mask.clear();
    if (text == nullptr) text = "";
    const char* p = text;
    while (*p) {
        if (isspace((unsigned char) *p) || *p == ',') {
            p++;
            continue;
        }
        if (*p == '"') {
            p++;
            bool closed = false;
            while (*p) {
                if (*p == '"') {
                    closed = true;
                    p++;
                    break;
                }
                if (*p == '\\' && (p[1] == '"' || p[1] == '\\')) p++;
                out.bytes.push_back((uint8_t) *p);
                out.mask.push_back(1);
                p++;
            }
            if (!closed) return fail(error, "Missing closing quote");
            continue;
        }
        if (*p == '?') {
            if (!allow_wildcards) return fail(error, "Wildcards are not allowed here");
            p++;
            if (*p == '?') p++;
            out.bytes.push_back(0);
            out.mask.push_back(0);
            continue;
        }
        /* Hex token: optional $ or 0x prefix, then a run of hex digits */
        if (*p == '$') p++;
        else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && hex_digit(p[2]) >= 0) p += 2;
        const char* start = p;
        while (hex_digit(*p) >= 0) p++;
        const size_t n = (size_t) (p - start);
        if (n == 0 || (*p && !isspace((unsigned char) *p) && *p != ',' && *p != '"' && *p != '?')) {
            return fail(error, "Use hex bytes (3E 22 ?\?) or a \"string\"");
        }
        if (n == 1) {
            out.bytes.push_back((uint8_t) hex_digit(start[0]));
            out.mask.push_back(1);
            continue;
        }
        if (n % 2) return fail(error, "Odd number of hex digits");
        for (size_t i = 0; i < n; i += 2) {
            out.bytes.push_back((uint8_t) (hex_digit(start[i]) << 4 | hex_digit(start[i + 1])));
            out.mask.push_back(1);
        }
    }
    if (out.bytes.empty()) return fail(error, "Empty pattern");
    return true;
}

int find(const uint8_t* mem, int size, const Pattern& p, int from, bool forward, bool* wrapped)
{
    if (wrapped) *wrapped = false;
    const int n = p.size();
    if (n == 0 || n > size) return -1;
    const int count = size - n + 1; /* possible start positions */
    int start;
    if (forward) start = from < 0 ? 0 : from >= count ? 0 : from;
    else start = from >= count ? count - 1 : from < 0 ? count - 1 : from;
    for (int k = 0; k < count; k++) {
        const int pos = forward ? (start + k) % count : (start - k + count) % count;
        if (p.matches(mem + pos)) {
            if (wrapped) *wrapped = forward ? pos < from : pos > from;
            return pos;
        }
    }
    return -1;
}

bool parse_byte(const char* text, uint8_t* out)
{
    if (text == nullptr) return false;
    while (isspace((unsigned char) *text)) text++;
    char* end = nullptr;
    unsigned long v;
    if (*text == '#') {
        if (!isdigit((unsigned char) text[1])) return false;
        v = strtoul(text + 1, &end, 10);
    } else {
        if (*text == '$') text++;
        else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
        if (hex_digit(*text) < 0) return false;
        v = strtoul(text, &end, 16);
    }
    while (end && isspace((unsigned char) *end)) end++;
    if (end == nullptr || *end != '\0' || v > 0xFF) return false;
    *out = (uint8_t) v;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Changed-bytes search                                               */
/* ------------------------------------------------------------------ */

} // namespace zui::memtools
