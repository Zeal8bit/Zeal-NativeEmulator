#include "debugger/api.h"
#include "ui/fltk/dock.h"
#include "ui/fltk/input.h"
#include "ui/fltk/theme.h"
#include <FL/Enumerations.H>
#include <cassert>
#include <filesystem>
#include <fstream>
int main()
{
    assert(fltk_key_to_host('a') == 65);
    assert(fltk_key_to_host(FL_F + 11) == 300);
    assert(fltk_key_to_host(FL_Shift_R) == 344);
    assert(fltk_key_to_host(FL_KP_Enter) == 335);
    assert(fltk_key_to_host(0x123456) == 0);
    using namespace zeal_ui;
    Workspace w;
    assert(w.leaf(0) && w.leaf(6) && !w.leaf(7));
    w.show(7);
    assert(w.leaf(7));
    w.dock(0, 3, 0);
    assert(w.leaf(0) == w.leaf(3));
    w.dock(0, 3, 1);
    assert(w.leaf(0) != w.leaf(3));
    w.detach(0, 30, 40);
    assert(!w.leaf(0) && w.floating.size() == 1);
    assert(w.save("workspace-test.ini"));
    Workspace r;
    assert(r.load("workspace-test.ini"));
    assert(r.floating[0].panel == 0);
    r.hide(0);
    assert(r.floating.empty());
    r.show(0);
    assert(r.leaf(0));
    std::ofstream("workspace-test.ini")
        << "version=1\nhidden=0\nroot=1,0.5,0\nroota=0,0.5,0,0\nrootb=0,0.5,0,0\n";
    assert(!r.load("workspace-test.ini") && r.leaf(0));
    std::ofstream("theme-test.ini") << "version=1\nbase=Light\ntext=#123456\nfont_size=18\nrow_height=30\n";
    Theme t = Theme::preset(false);
    std::string error;
    assert(Theme::load("theme-test.ini", t, error));
    assert(t.colors.at("text") == 0x123456 && t.font_size == 18);
    std::ofstream("theme-test.ini") << "version=1\nbase=Dark\ntext=#nothex\n";
    assert(!Theme::load("theme-test.ini", t, error) && t.colors.at("text") == 0x123456);
    std::filesystem::remove("workspace-test.ini");
    std::filesystem::remove("theme-test.ini");
    return 0;
}
