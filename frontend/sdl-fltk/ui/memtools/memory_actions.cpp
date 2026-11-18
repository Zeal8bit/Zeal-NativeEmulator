/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "memtools/memory_actions.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <FL/Fl_Native_File_Chooser.H>
#include "common/format.h"
#include "common/machine.h"
#include "debugger/address_lookup.h"
#include "memtools/form_dialog.h"
#include "memtools/pattern.h"

namespace zui::memtools {

using namespace zui::common;
using namespace zui::debugger;

namespace {

std::string hex16s(unsigned v)
{
    char t[8];
    hex16(t, sizeof(t), v);
    return t;
}

bool parse_range(FormDialog& d, int first, unsigned* start, unsigned* end)
{
    hwaddr s, e;
    if (!parse_address(d.value(first), &s)) {
        d.error("Start: hex address ($C000) or symbol");
        return false;
    }
    if (!parse_address(d.value(first + 1), &e)) {
        d.error("End: hex address ($C0FF) or symbol");
        return false;
    }
    if (s > 0xFFFF || e > 0xFFFF || e < s) {
        d.error("End must be at or after Start, both within $0000-$FFFF");
        return false;
    }
    *start = (unsigned) s;
    *end = (unsigned) e;
    return true;
}

std::string range_text(unsigned start, unsigned end)
{
    char t[64];
    snprintf(t, sizeof(t), "$%s-$%s (%u bytes)", hex16s(start).c_str(), hex16s(end).c_str(), end - start + 1);
    return t;
}

} // namespace

void fill_dialog(unsigned start, unsigned end, const Writer& write)
{
    FormDialog d("FILL MEMORY", "Fill", { "Start $", "End $", "Value" },
                 "Value: a hex byte (00), a byte pattern (DE AD BE EF) or a \"string\", repeated over the range.");
    d.value(0, hex16s(start).c_str());
    d.value(1, hex16s(end).c_str());
    d.value(2, "00");
    unsigned s = start, e = end;
    Pattern pattern;
    const bool ok = d.run([&](FormDialog& f) {
        if (!parse_range(f, 0, &s, &e)) return false;
        std::string err;
        if (!parse_pattern(f.value(2), pattern, &err, false)) {
            f.error(("Value: " + err).c_str());
            return false;
        }
        return true;
    });
    if (!ok) return;
    std::vector<uint8_t> data(e - s + 1);
    for (size_t i = 0; i < data.size(); i++) data[i] = pattern.bytes[i % pattern.bytes.size()];
    write(s, data);
    notif_show("Filled %s", range_text(s, e).c_str());
}

void save_range_dialog(unsigned start, unsigned end)
{
    FormDialog d("SAVE RANGE", "Save...", { "Start $", "End $" },
                 "Saves the bytes as the CPU sees them (current MMU mapping) to a binary file.");
    d.value(0, hex16s(start).c_str());
    d.value(1, hex16s(end).c_str());
    unsigned s = start, e = end;
    if (!d.run([&](FormDialog& f) { return parse_range(f, 0, &s, &e); })) return;

    Fl_Native_File_Chooser chooser;
    chooser.title("Save Memory Range");
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM | Fl_Native_File_Chooser::NEW_FOLDER);
    char name[64];
    snprintf(name, sizeof(name), "mem_%s_%s.bin", hex16s(s).c_str(), hex16s(e).c_str());
    chooser.preset_file(name);
    if (chooser.show() != 0 || chooser.filename() == nullptr) return;

    std::vector<uint8_t> data(e - s + 1);
    debugger_read_memory(fz_debugger(), (hwaddr) s, (int) data.size(), data.data());
    FILE* f = fopen(chooser.filename(), "wb");
    const bool written = f && fwrite(data.data(), 1, data.size(), f) == data.size();
    if (f && fclose(f) != 0) {
        notif_show("Could not write %s", chooser.filename());
        return;
    }
    if (written) notif_show("Saved %s", range_text(s, e).c_str());
    else notif_show("Could not write %s", chooser.filename());
}

void load_file_dialog(unsigned addr, const Writer& write)
{
    Fl_Native_File_Chooser chooser;
    chooser.title("Load File into Memory");
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    if (chooser.show() != 0 || chooser.filename() == nullptr) return;
    const std::string path = chooser.filename();

    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        notif_show("Could not open %s", path.c_str());
        return;
    }
    std::vector<uint8_t> data;
    uint8_t chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0 && data.size() <= 0x10000) {
        data.insert(data.end(), chunk, chunk + n);
    }
    fclose(f);
    if (data.empty()) {
        notif_show("%s is empty", path.c_str());
        return;
    }
    const bool huge = data.size() > 0x10000;
    if (huge) data.resize(0x10000);

    const size_t slash = path.find_last_of("/\\");
    const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    char note[256];
    snprintf(note, sizeof(note), "%s: %s%zu bytes", base.c_str(), huge ? "first " : "", data.size());
    FormDialog d("LOAD FILE", "Load", { "Address $" }, note);
    d.value(0, hex16s(addr).c_str());
    unsigned at = addr;
    std::string confirmed; /* address text for which truncation was accepted */
    const bool ok = d.run([&](FormDialog& form) {
        hwaddr a;
        if (!parse_address(form.value(0), &a) || a > 0xFFFF) {
            form.error("Address: hex ($C000) or symbol");
            return false;
        }
        at = (unsigned) a;
        const size_t fits = 0x10000 - at;
        if (data.size() > fits && confirmed != form.value(0)) {
            char t[160];
            snprintf(t, sizeof(t), "Crosses $FFFF: only %zu of %zu bytes fit ($%s-$FFFF). Press Load again to load them.",
                     fits, data.size(), hex16s(at).c_str());
            form.warn(t);
            confirmed = form.value(0);
            return false;
        }
        return true;
    });
    if (!ok) return;
    const size_t fits = 0x10000 - at;
    if (data.size() > fits) data.resize(fits);
    write(at, data);
    notif_show("Loaded %s at %s", base.c_str(), range_text(at, at + (unsigned) data.size() - 1).c_str());
}

bool address_dialog(const char* title, unsigned initial, unsigned* out)
{
    FormDialog d(title, "Go", { "Address $" }, "Hex address ($C000) or a debugger symbol.");
    d.value(0, hex16s(initial).c_str());
    hwaddr a = 0;
    const bool ok = d.run([&](FormDialog& f) {
        if (parse_address(f.value(0), &a) && a <= 0xFFFF) return true;
        f.error("Not a hex address or known symbol");
        return false;
    });
    if (ok) *out = (unsigned) a;
    return ok;
}

/* ------------------------------------------------------------------ */
/*  Changed-bytes search window                                        */
/* ------------------------------------------------------------------ */

} // namespace zui::memtools
