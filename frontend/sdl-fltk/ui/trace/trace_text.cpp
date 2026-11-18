/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "trace/trace_text.h"
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl_Native_File_Chooser.H>
#include "common/format.h"
#include "common/machine.h"
#include "frontend/sdl-fltk/bridge/trace.h"
#include "trace/symbol_index.h"

namespace zui::trace {

using namespace zui::common;

const char* event_name(uint8_t kind)
{
    switch (kind) {
        case ZEAL_TRACE_NMI: return "NMI";
        case ZEAL_TRACE_IM0: return "IRQ IM0";
        case ZEAL_TRACE_IM1: return "IRQ IM1";
        case ZEAL_TRACE_IM2: return "IRQ IM2";
        case ZEAL_TRACE_RESET: return "RESET";
        case ZEAL_TRACE_GAP: return "UNTRACED";
        default: return nullptr;
    }
}

void event_text(char* out, size_t size, const zeal_trace_entry_t& e, const SymbolIndex& syms)
{
    const char* name = event_name(e.kind);
    if (name == nullptr) name = "?";
    switch (e.kind) {
        case ZEAL_TRACE_RESET:
            snprintf(out, size, "-- RESET --");
            return;
        case ZEAL_TRACE_GAP:
            snprintf(out, size, "-- code ran untraced (debugger or trace off) --");
            return;
        default: {
            char h[8];
            char sym[96];
            hex16(h, sizeof(h), e.target);
            syms.describe(sym, sizeof(sym), e.target);
            snprintf(out, size, "-- %s -> $%s%s%s --", name, h, sym[0] ? " " : "", sym);
            return;
        }
    }
}

void bytes_text(char* out, size_t size, const zeal_trace_entry_t& e, int length)
{
    if (length < 1 || length > 4) length = 4;
    size_t used = 0;
    out[0] = 0;
    for (int i = 0; i < length && used + 4 < size; i++) {
        char b[4];
        hex8(b, sizeof(b), e.bytes[i]);
        used += (size_t) snprintf(out + used, size - used, i ? " %s" : "%s", b);
    }
}

std::string trace_header()
{
    return "     T-STATE  PC    BYTES        INSTRUCTION             "
           "AF   BC   DE   HL   IX   IY   SP    PG  SYMBOL";
}

std::string trace_line(const zeal_trace_entry_t& e, const SymbolIndex& syms)
{
    char line[256];
    char pc[8];
    char sym[96];
    hex16(pc, sizeof(pc), e.pc);
    syms.describe(sym, sizeof(sym), e.pc);
    char text[96];
    char bytes[16] = "";
    if (e.kind == ZEAL_TRACE_INSTR) {
        dbg_instr_t instr;
        const int size = fz_trace_disassemble(&e, &instr);
        bytes_text(bytes, sizeof(bytes), e, size);
        snprintf(text, sizeof(text), "%s", instr.instruction);
    } else {
        event_text(text, sizeof(text), e, syms);
    }
    char r[7][8];
    const uint16_t regs[7] = { e.af, e.bc, e.de, e.hl, e.ix, e.iy, e.sp };
    for (int i = 0; i < 7; i++) hex16(r[i], sizeof(r[i]), regs[i]);
    char pg[4];
    hex8(pg, sizeof(pg), e.page);
    snprintf(line, sizeof(line), "%12" PRIu64 "  %s  %-11s  %-22s  %s %s %s %s %s %s %s  %s  %s",
             e.cycle, pc, bytes, text, r[0], r[1], r[2], r[3], r[4], r[5], r[6], pg, sym);
    return line;
}

void save_trace_dialog(const std::vector<zeal_trace_entry_t>& newest_first, const SymbolIndex& syms)
{
    if (newest_first.empty()) {
        notif_show("The trace is empty");
        return;
    }
    Fl_Native_File_Chooser chooser;
    chooser.title("Save Execution Trace");
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM | Fl_Native_File_Chooser::NEW_FOLDER);
    chooser.preset_file("trace.txt");
    if (chooser.show() != 0 || chooser.filename() == nullptr) return;

    FILE* f = fopen(chooser.filename(), "w");
    if (f == nullptr) {
        notif_show("Could not write %s", chooser.filename());
        return;
    }
    fprintf(f, "; Zeal execution trace, oldest first (%zu entries). Registers are before each instruction.\n",
            newest_first.size());
    fprintf(f, "%s\n", trace_header().c_str());
    for (auto it = newest_first.rbegin(); it != newest_first.rend(); ++it) {
        fprintf(f, "%s\n", trace_line(*it, syms).c_str());
    }
    const bool ok = !ferror(f);
    if (fclose(f) != 0 || !ok) {
        notif_show("Could not write %s", chooser.filename());
        return;
    }
    notif_show("Saved %zu trace entries", newest_first.size());
}

} // namespace zui::trace
