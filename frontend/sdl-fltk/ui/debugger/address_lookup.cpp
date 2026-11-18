/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/address_lookup.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Widget.H>
#include "common/format.h"
#include "common/machine.h"
#include "theme/theme.h"
#include "memtools/pattern.h"

namespace zui::debugger {

using namespace zui::memtools;
using namespace zui::common;
using namespace zui::theme;

namespace {

std::string loose_name(const char* s)
{
    while (*s == '_') s++;
    std::string out(s);
    for (char& c : out) c = (char) tolower((unsigned char) c);
    return out;
}

} // namespace

bool parse_address(const char* text, hwaddr* out)
{
    if (text == nullptr || out == nullptr) return false;
    while (*text == ' ' || *text == '\t') text++;
    if (*text == '\0') return false;

    const char* digits = text;
    if (*digits == '$') digits++;
    else if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits += 2;
    char* end = nullptr;
    const unsigned long value = std::strtoul(digits, &end, 16);
    if (end != digits) {
        while (*end == ' ' || *end == 'h' || *end == 'H') end++;
        if (*end == '\0') {
            *out = (hwaddr) (value & 0xFFFF);
            return true;
        }
    }
    dbg_t* dbg = fz_debugger();
    return dbg != nullptr && debugger_find_symbol(dbg, text, out);
}

bool lookup_address(Fl_Widget* anchor, const char* text, hwaddr* out, int prefer_kind, std::string* picked)
{
    if (picked) picked->clear();
    if (text == nullptr || out == nullptr) return false;
    std::string query(text);
    while (!query.empty() && isspace((unsigned char) query.back())) query.pop_back();
    size_t lead = 0;
    while (lead < query.size() && isspace((unsigned char) query[lead])) lead++;
    query.erase(0, lead);
    if (query.empty()) return false;

    dbg_t* dbg = fz_debugger();
    /* An exact symbol name wins over hex ("add", "beef" can be labels) */
    if (dbg && debugger_find_symbol(dbg, query.c_str(), out)) {
        if (picked) *picked = query;
        return true;
    }
    if (parse_address(query.c_str(), out)) return true;
    if (dbg == nullptr) return false;

    /* Rank: 0 same name ignoring case, 1 ignoring leading underscores,
     * 2 prefix, 3 substring (all ignoring case and leading underscores) */
    struct Match {
        int rank;
        const symbol_t* sym;
    };
    std::vector<Match> matches;
    std::string lower = query;
    for (char& c : lower) c = (char) tolower((unsigned char) c);
    const std::string loose = loose_name(query.c_str());
    for (const symbols_t* b = &dbg->symbols; b; b = b->next) {
        for (unsigned i = 0; i < b->count; i++) {
            const symbol_t& s = b->array[i];
            if (s.name == nullptr) continue;
            std::string name(s.name);
            for (char& c : name) c = (char) tolower((unsigned char) c);
            const std::string ln = loose_name(s.name);
            int rank = -1;
            if (name == lower) rank = 0;
            else if (!loose.empty() && ln == loose) rank = 1;
            else if (!loose.empty() && ln.compare(0, loose.size(), loose) == 0) rank = 2;
            else if (!loose.empty() && ln.find(loose) != std::string::npos) rank = 3;
            if (rank >= 0) matches.push_back({ rank, &s });
        }
    }
    if (matches.empty()) {
        notif_show("No symbol matches \"%s\"", query.c_str());
        return false;
    }
    std::stable_sort(matches.begin(), matches.end(), [&](const Match& a, const Match& b) {
        if (a.rank != b.rank) return a.rank < b.rank;
        const bool pa = a.sym->kind == prefer_kind, pb = b.sym->kind == prefer_kind;
        if (pa != pb) return pa;
        if (a.sym->local != b.sym->local) return !a.sym->local;
        return strcmp(a.sym->name, b.sym->name) < 0;
    });

    /* A single best name match (or a single match at all) needs no menu */
    const bool unique_best = matches[0].rank <= 1 && (matches.size() == 1 || matches[1].rank > matches[0].rank);
    if (matches.size() == 1 || unique_best) {
        *out = matches[0].sym->addr;
        if (picked) *picked = matches[0].sym->name;
        return true;
    }

    /* Several candidates: let the user pick */
    constexpr size_t MAX_ITEMS = 30;
    const size_t shown = std::min(matches.size(), MAX_ITEMS);
    std::vector<std::string> labels(shown + 2);
    std::vector<Fl_Menu_Item> items(shown + 3);
    memset(items.data(), 0, sizeof(Fl_Menu_Item) * items.size());
    char head[96];
    snprintf(head, sizeof(head), "%zu symbols match \"%s\"%s", matches.size(), query.c_str(),
             matches.size() > shown ? " (type more to narrow)" : "");
    labels[0] = head;
    items[0].text = labels[0].c_str();
    items[0].flags = FL_MENU_INACTIVE | FL_MENU_DIVIDER;
    char addr[16];
    for (size_t i = 0; i < shown; i++) {
        const symbol_t* s = matches[i].sym;
        hex16(addr, sizeof(addr), s->addr);
        const char* kind = s->kind == SYM_KIND_CODE ? "code" : s->kind == SYM_KIND_DATA ? "data"
                         : s->kind == SYM_KIND_CONST ? "const" : "";
        std::string label = std::string(s->name) + "    $" + addr + (kind[0] ? "  " : "") + kind;
        /* '&' would underline in menus, '/' and '\\' are fine in popups */
        std::string safe;
        for (char c : label) {
            if (c == '&') safe += "&&";
            else safe += c;
        }
        labels[i + 1] = safe;
        items[i + 1].text = labels[i + 1].c_str();
        items[i + 1].labelfont_ = DATA_FONT;
        items[i + 1].labelsize_ = DATA_SIZE;
        items[i + 1].labelcolor_ = s->local ? COLOR_DIM : COLOR_TEXT;
    }
    const int px = anchor ? anchor->x() : Fl::event_x();
    const int py = anchor ? anchor->y() + anchor->h() : Fl::event_y();
    const Fl_Menu_Item* choice = items.data()->popup(px, py);
    if (choice == nullptr || choice == items.data()) return false;
    const symbol_t* s = matches[(size_t) (choice - items.data()) - 1].sym;
    *out = s->addr;
    if (picked) *picked = s->name;
    return true;
}

} // namespace zui::debugger
