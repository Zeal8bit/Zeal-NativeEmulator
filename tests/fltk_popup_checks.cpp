// SPDX-License-Identifier: Apache-2.0
#include "fltk_popup_checks.h"
#include <FL/Fl.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Window.H>
#include <cassert>
#include <cstdio>
#ifdef __APPLE__
#include <CoreGraphics/CGWindow.h>
#endif

namespace
{
struct PopupCheck {
    Fl_Window &owner;
    int header_height;
    bool checked = false;
};

#ifdef __APPLE__
CGRect native_bounds(Fl_Window &window)
{
    auto info = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, window.os_id());
    assert(info && CFArrayGetCount(info));
    auto entry = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(info, 0));
    CGRect result;
    assert(CGRectMakeWithDictionaryRepresentation(
        static_cast<CFDictionaryRef>(CFDictionaryGetValue(entry, kCGWindowBounds)), &result));
    CFRelease(info);
    return result;
}
#endif

void inspect_and_close(void *data)
{
    auto &check = *static_cast<PopupCheck *>(data);
    auto &owner = check.owner;
    assert(Fl::grab() && Fl::grab() != &owner);
    int dropdowns = 0;
    for (auto w = Fl::first_window(); w; w = Fl::next_window(w)) {
        if (!w->menu_window())
            continue;
        assert(w->screen_num() == owner.screen_num());
        // These test windows are centered with enough room for every menu.
        assert(w->x() >= owner.x() - 4 && w->x() + w->w() <= owner.x() + owner.w() + 4);
        assert(w->y() >= owner.y() - 4 && w->y() + w->h() <= owner.y() + owner.h() + 4);
#ifdef __APPLE__
        const auto popup = native_bounds(*w), parent = native_bounds(owner);
        assert(CGRectContainsRect(CGRectInset(parent, -4, -4), popup));
#endif
        dropdowns += w->h() > check.header_height;
    }
    assert(dropdowns);
    check.checked = true;
    Fl::e_keysym = FL_Escape;
    Fl::handle(FL_KEYBOARD, Fl::grab());
}
} // namespace

void fltk_check_menu_placement(Fl_Window &owner, Fl_Menu_Bar &menu)
{
    int x = menu.x() + 6;
    unsigned count = 0;
    for (auto item = menu.menu(); item->text; item = item->next()) {
        PopupCheck check{owner, menu.h()};
        Fl::add_timeout(.03, inspect_and_close, &check);
        Fl::e_x = x + 8;
        Fl::e_y = menu.y() + menu.h() / 2;
        Fl::e_x_root = owner.x() + Fl::e_x;
        Fl::e_y_root = owner.y() + Fl::e_y;
        Fl::e_keysym = FL_Button + 1;
        Fl::e_state = FL_BUTTON1;
        Fl::e_is_click = 1;
        Fl::handle(FL_PUSH, &owner);
        Fl::remove_timeout(inspect_and_close, &check);
        assert(check.checked);
        Fl::e_state = 0;
        Fl::handle(FL_RELEASE, &owner);
        x += item->measure(nullptr, &menu) + 16;
        ++count;
    }
    std::printf("FLTK_POPUP_PLACEMENT_OK screen=%d menus=%u\n", owner.screen_num(), count);
}
