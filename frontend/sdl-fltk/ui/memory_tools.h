/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Memory editor tools (memory_tools.cpp): byte-pattern search, the
 * "changed since last pause" candidate search, and the dialogs used by the
 * memory view's context menu. The search engines are plain C++ with no
 * FLTK or machine dependency so they can be unit-tested on their own. */
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace zui {
namespace memtools {

/* ------------------------------------------------------------------ */
/*  Pattern search                                                     */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/*  "Changed since last pause" candidate search                        */
/* ------------------------------------------------------------------ */

enum class Filter { Changed, Unchanged, Increased, Decreased, Equal };

class ChangeSearch {
public:
    /** Begin a new search: every address of `size` is a candidate. */
    void start(int size = 0x10000);
    void stop() { m_active = false; m_candidates.clear(); m_rounds = 0; }
    bool active() const { return m_active; }
    /** Keep the candidates for which `prev` -> `cur` passes the filter. */
    void narrow(Filter filter, uint8_t value, const uint8_t* prev, const uint8_t* cur);
    const std::vector<uint16_t>& candidates() const { return m_candidates; }
    int rounds() const { return m_rounds; }

    static bool passes(Filter filter, uint8_t value, uint8_t prev, uint8_t cur);

private:
    std::vector<uint16_t> m_candidates;
    bool m_active = false;
    int m_rounds = 0;
};

/* ------------------------------------------------------------------ */
/*  Dialogs and tool windows (debugger builds only)                    */
/* ------------------------------------------------------------------ */

/** Write bytes to memory through the memory view (keeps its snapshot). */
using Writer = std::function<void(unsigned addr, const std::vector<uint8_t>& data)>;

/** Fill dialog: start/end prefilled, value byte or pattern; writes on OK. */
void fill_dialog(unsigned start, unsigned end, const Writer& write);
/** Save dialog: start/end prefilled, then a native file chooser. */
void save_range_dialog(unsigned start, unsigned end);
/** File chooser, then a confirmation with the destination (warns at $FFFF). */
void load_file_dialog(unsigned addr, const Writer& write);
/** Ask for an address or symbol; returns false on cancel. */
bool address_dialog(const char* title, unsigned initial, unsigned* out);

/** Snapshots the changed-bytes search compares (owned by the memory view). */
struct SnapshotSource {
    /** Previous and current pause images (64 KB each). False before the
     *  first pause; `prev` is null while only one pause has been seen. */
    std::function<bool(const uint8_t** prev, const uint8_t** cur)> get;
    std::function<void(unsigned addr)> go;
};

/** Open (or raise) the changed-bytes search window. */
void change_search_show(const SnapshotSource& source);
/** Called after the memory view took a new pause snapshot. */
void change_search_paused();
void change_search_destroy();

} // namespace memtools
} // namespace zui
