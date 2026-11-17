/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* TEMPORARY placeholder until the debugger panels land: empty docks. */

#include <FL/Fl_Tabs.H>
#include "ui.h"

namespace zui {

struct Debugger::Impl {
    DockLayout* dock;
};

Debugger::Debugger(DockLayout* dock) : m(new Impl{dock})
{
    dock->begin();
    Fl_Box* right = new Fl_Box(0, 0, 10, 10, "Debugger panels: work in progress");
    right->box(FL_FLAT_BOX);
    right->color(COLOR_PANEL);
    right->labelcolor(COLOR_DIM);
    Fl_Box* bottom = new Fl_Box(0, 0, 10, 10);
    bottom->box(FL_FLAT_BOX);
    bottom->color(COLOR_PANEL);
    dock->end();
    Fl_Widget* screen = dock->child(0);
    dock->set_children(screen, right, bottom);
}

Debugger::~Debugger() { delete m; }

Fl_Group* Debugger::toolbar(int x, int y, int w, int h)
{
    Fl_Group* g = new Fl_Group(x, y, w, h);
    g->box(FL_FLAT_BOX);
    g->color(COLOR_PANEL);
    g->end();
    return g;
}

void Debugger::refresh(bool) {}
void Debugger::goto_memory(hwaddr) {}
void Debugger::goto_disassembly(hwaddr) {}
void Debugger::apply_panel_visibility() {}
void Debugger::reset_layout() {}
void Debugger::capture_layout() {}

} // namespace zui
