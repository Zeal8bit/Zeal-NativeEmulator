/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Watch expressions of the Watch panel: their types, evaluation against
 * the machine state, formatting and persistence (FLTK_WATCH_* keys). */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "common/machine.h"

namespace zui::debugger {

enum WatchType { W_U8, W_S8, W_U16, W_S16, W_STR, W_PTR8, W_PTR16, W_BITS, W_COUNT };

struct TypeInfo {
    const char* key;   /* persisted name */
    const char* tag;   /* short tag drawn in the row and in the type choice */
    const char* label; /* context menu */
};

/* Each watch is saved as one FLTK_WATCH_<n> config value; rini holds
 * RINI_MAX_VALUE_CAPACITY (512, frontend/common/config.c) values in all and
 * the rest of the configuration uses about 100. */
constexpr int MAX_WATCHES = 64;
constexpr int MAX_EXPR = 48;
constexpr int MAX_STR = 64;
constexpr int DEFAULT_STR = 16;

constexpr int MAX_BYTES = MAX_STR + 2;

struct Watch {
    std::string expr;
    int type = W_U8;
    int len = DEFAULT_STR;
    /* Break on access (watch_breaks.h): WATCHPOINT_* bits, 0 = never */
    uint8_t brk = 0;
    bool brk_on = true;
    /* Evaluated at each refresh */
    bool valid = false;
    hwaddr addr = 0;
    hwaddr target = 0; /* pointer types: the address pointed to */
    uint8_t cur[MAX_BYTES] = {};
    int n = 0;
    /* Snapshot taken at the previous pause */
    bool has_prev = false;
    hwaddr prev_addr = 0;
    uint8_t prev[MAX_BYTES] = {};
    int prev_n = 0;
    bool changed = false;
    bool addr_changed = false;
    uint8_t bits_changed = 0; /* bitfields: bits that changed at the last pause */
};

/** Labels and config keys of each WatchType. */
extern const TypeInfo WATCH_TYPES[W_COUNT];

/* The list lives outside the panel so that it can be saved at exit even
 * after the debugger window has been destroyed. */
std::vector<Watch>& watches();

/** Address designated by an expression (see the top of the file). */
bool eval_expr(const std::string& text, const regs_t& regs, hwaddr* out, int depth = 0);

void evaluate(Watch& w, const regs_t& regs);

bool is_pointer(const Watch& w);

/** Address shown in the ADDR column: where the displayed value lives
 *  (the target for pointers, whose own location is the expression). */
hwaddr shown_addr(const Watch& w);

/** Remember the current value as the reference for change highlighting. */
void take_snapshot(Watch& w);

/** HEX and DEC cell texts. Strings use `hex` only (quoted text). */
void format(const Watch& w, char* hex, size_t hs, char* dec, size_t ds);

void watches_load();

/** Add a watch of `type` at `addr` (memory view), named after the symbol at
 *  that address when there is one. `len` is the string length. */
bool watch_add_at(hwaddr addr, int type, int len);

} // namespace zui::debugger
