// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <memory>
#include <string>
#include <vector>
namespace zeal_ui
{
// Logical model independent of FLTK widget lifetime. Leaves own ordered panel IDs.
struct DockNode {
    int axis = 0; // 0 tabs, 1 horizontal split, 2 vertical split
    double ratio = .5;
    int selected = 0;
    std::vector<int> tabs;
    std::unique_ptr<DockNode> first, second;
};
struct Floating {
    int panel, x = 80, y = 80, w = 640, h = 480;
};
struct Workspace {
    std::unique_ptr<DockNode> root;
    std::vector<Floating> floating;
    unsigned hidden = 128;
    Workspace();
    void reset();
    bool remove(int panel);
    void dock(int panel, int target, int edge); // 0 tab, 1 left, 2 right, 3 top, 4 bottom
    void detach(int panel, int x, int y);
    void hide(int panel);
    void show(int panel);
    // Deep copy of the dock tree, so a layout can be set aside and put back later.
    Workspace clone() const;
    bool save(const std::string &file) const;
    bool load(const std::string &file);
    DockNode *leaf(int panel) const;
};
} // namespace zeal_ui
