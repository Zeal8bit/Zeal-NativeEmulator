// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/dock.h"
#include <FL/Fl_Group.H>
#include <vector>

struct dbg_ui_t;

struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const
    {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

// The dockable workspace. It turns the workspace tree into leaf rectangles, draws the
// per-leaf tab strips and split handles, and reparents panels as the layout changes.
class DockSurface : public Fl_Group
{
  public:
    struct Leaf {
        zeal_ui::DockNode *node;
        Rect rect;
    };
    struct Split {
        zeal_ui::DockNode *node;
        Rect rect, full;
    };
    dbg_ui_t *ui;
    std::vector<Leaf> leaves;
    std::vector<Split> splits;
    zeal_ui::DockNode *resizing = nullptr;
    Rect resize_rect{};

    DockSurface(dbg_ui_t *u, int x, int y, int w, int h);
    void arrange(zeal_ui::DockNode *n, Rect r);
    void update();
    void resize(int x, int y, int w, int h) override;
    void draw() override;
    int handle(int event) override;
    // Which edge of a leaf a drop at (x, y) would split: 1 left, 2 right, 3 top,
    // 4 bottom, 0 the tab strip in the middle.
    static int target_edge(Rect r, int x, int y);
};
