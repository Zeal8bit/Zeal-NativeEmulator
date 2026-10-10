#include "debugger/api.h"
#include "host/zeal_host.h"
#include "ui/fltk/dock.h"
#include "ui/fltk/input.h"
#include "ui/fltk/theme.h"
#include <FL/Enumerations.H>
#include <cassert>
#include <filesystem>
#include <fstream>
int main()
{
    /* The frontend speaks the host seam's key codes, not any windowing library's. */
    assert(fltk_key_to_host('a') == ZEAL_HOST_KEY_A);
    assert(fltk_key_to_host('Z') == ZEAL_HOST_KEY_Z);
    assert(fltk_key_to_host('7') == ZEAL_HOST_KEY_SEVEN);
    assert(fltk_key_to_host(';') == ZEAL_HOST_KEY_SEMICOLON);
    assert(fltk_key_to_host(' ') == ZEAL_HOST_KEY_SPACE);
    assert(fltk_key_to_host(FL_F + 11) == ZEAL_HOST_KEY_F11);
    assert(fltk_key_to_host(FL_Shift_R) == ZEAL_HOST_KEY_RIGHT_SHIFT);
    assert(fltk_key_to_host(FL_KP_Enter) == ZEAL_HOST_KEY_KP_ENTER);
    assert(fltk_key_to_host(0x123456) == ZEAL_HOST_KEY_NONE);
    /* Every key the frontend can produce must be a real seam code, and the ones the
     * emulator acts on must not collide with "no key". */
    for (int k = 0; k < 0x110000; k += 7) {
        unsigned mapped = fltk_key_to_host(k);
        assert(mapped == ZEAL_HOST_KEY_NONE || mapped < ZEAL_HOST_KEY_COUNT);
    }
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
