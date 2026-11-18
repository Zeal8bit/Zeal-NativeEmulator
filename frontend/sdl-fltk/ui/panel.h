/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Building blocks shared by every debugger panel and tool view:
 * the Panel base class, styling, formatting, register access and
 * cross-panel navigation. New panels/views include this header only. */
#pragma once

#include <functional>
#include <string>
#include <FL/Fl_Input.H>
#include <FL/Fl_Check_Button.H>
#include "ui.h"

namespace zui {

constexpr int BAR_H = 30; /* input/button rows on top of panels */

/* ------------------------------------------------------------------ */
/*  Formatting and drawing helpers (common.cpp)                        */
/* ------------------------------------------------------------------ */

void hex16(char* out, size_t size, unsigned value);
void hex8(char* out, size_t size, unsigned value);
/** Width of one character of the data font (set fl_font itself). */
int  char_width();
/** Baseline offset of data-font text inside a line of `line_h` pixels. */
int  text_baseline(int line_h);
void style_input(Fl_Input* in);
void style_button(Fl_Button* b);
void style_check(Fl_Check_Button* b);

/* ------------------------------------------------------------------ */
/*  Machine access helpers (common.cpp)                                */
/* ------------------------------------------------------------------ */

/** True once the debugger is available (machine bound, debugger built). */
bool   dbg_ready();
/** Registers to display: exact when paused, the mid-frame sample while
 *  running (reading them between frames always lands on the frame boundary). */
regs_t current_regs();
/** Registers for editing: always the exact current state. */
regs_t exact_regs();
/** Memory at SP (stack=true) or HL as displayed: sampled while running. */
void   display_memory(hwaddr addr, int len, uint8_t* out, bool stack);

/* ------------------------------------------------------------------ */
/*  Navigation and refresh (implemented by the debugger)               */
/* ------------------------------------------------------------------ */

/** Refresh every debugger panel and tool view now (after a user action). */
void request_refresh();
/**
 * Resolve what the user typed in an address field: hex ($C000, 0xC000,
 * C000) or a symbol. Symbols are looked up loosely (case, leading
 * underscores, prefix, substring); several matches pop up a menu below
 * `anchor` to pick from, listing `prefer_kind` (symbol_kind_t, or -1)
 * first. `picked`, if given, receives the chosen symbol's name (empty for
 * hex). Returns false if nothing matched or the menu was dismissed.
 */
bool lookup_address(Fl_Widget* anchor, const char* text, hwaddr* out, int prefer_kind = -1,
                    std::string* picked = nullptr);

/** Show an address in the main window's memory editor / disassembly. */
void goto_memory(hwaddr addr);
void goto_disassembly(hwaddr addr);

/* ------------------------------------------------------------------ */
/*  Persistence: extra keys written by config_save() (common.cpp)      */
/* ------------------------------------------------------------------ */

/** Register a function that appends keys (use an FLTK_ prefix) when the
 *  configuration is saved. Read values back with config_get/_text. */
void persist_register(std::function<void(rini_config*)> save);

/* ------------------------------------------------------------------ */
/*  Panel: surface with a retro header strip                           */
/* ------------------------------------------------------------------ */

class Panel : public Fl_Group {
public:
    Panel(int X, int Y, int W, int H, const char* title)
        : Fl_Group(X, Y, W, H), m_title(title)
    {
        box(FL_FLAT_BOX);
        color(COLOR_SURFACE);
    }

    /** Right-aligned text in the header (state, counts...). */
    void extra(const char* text);
    void draw() override;

    /** First y below the header. */
    int top() const { return y() + HEADER_H; }
    const char* extra_text() const { return m_extra; }
    /**
     * Update the content. `paused`: the machine is stopped in the debugger.
     * `force`: a pause/step/user action happened (always update); otherwise
     * this is a periodic refresh while running (~10 Hz in the main window,
     * ~4 Hz in tool windows).
     */
    virtual void refresh(bool paused, bool force) = 0;

private:
    const char* m_title;
    char m_extra[64] = "";
};

/* ------------------------------------------------------------------ */
/*  Panels living in the main debugger window                          */
/* ------------------------------------------------------------------ */

/** Watch panel (view_watch.cpp), right column between Stack and Breakpoints. */
Panel* create_watch_panel(int x, int y, int w, int h);

/* ------------------------------------------------------------------ */
/*  Map files (view_symbols.cpp)                                       */
/* ------------------------------------------------------------------ */

/** Ask for a map file and load it: replaces the symbols, or adds to them
 *  when `append`. Returns true if a file was loaded. */
bool map_open_dialog(bool append);
/** Reload the loaded map files (after rebuilding the program). */
void map_reload();
/** Remove every symbol. */
void map_clear();

} // namespace zui
