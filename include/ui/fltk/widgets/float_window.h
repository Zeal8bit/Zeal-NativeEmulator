// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <FL/Fl_Double_Window.H>

struct dbg_ui_t;

// A panel that has been detached into its own top-level window. The strip at the top is
// the drag handle; double-clicking it re-docks the panel.
class FloatWindow : public Fl_Double_Window
{
  public:
    dbg_ui_t *ui;
    int id;

    FloatWindow(dbg_ui_t *u, int id, int x, int y, int w, int h);
    void draw() override;
    int handle(int event) override;
};
