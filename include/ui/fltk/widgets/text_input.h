// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Button.H>

struct dbg_ui_t;

// FLTK's Fl_Input implements cut/copy/paste/select-all/undo on keyboard shortcuts but
// deliberately ships no context menu, which is what makes its text fields feel
// non-native. One shared popup gives every text field the standard right-click menu.
// (Fl::add_handler cannot be used here: it only sees events FLTK does not otherwise
// recognise, so it never receives FL_PUSH.)
struct InputMenu {
    Fl_Menu_Button *menu = nullptr;
    dbg_ui_t *ui = nullptr;
    static void picked(Fl_Widget *, void *data);
};
// The one popup shared by every text field in the UI.
InputMenu &shared_input_menu();
// Creates that popup and parents it to the shell's window, which owns it from then on.
void install_input_menu(dbg_ui_t *ui);

class TextInput : public Fl_Input
{
  public:
    TextInput(int x, int y, int w, int h, const char *l = nullptr) : Fl_Input(x, y, w, h, l)
    {
    }

    int handle(int event) override;
};
