// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/widgets/text_input.h"
#include "ui/fltk/debugger_ui.h"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <cstdint>

InputMenu &shared_input_menu()
{
    static InputMenu menu;
    return menu;
}

void install_input_menu(dbg_ui_t *ui)
{
    InputMenu &input_menu = shared_input_menu();
    if (input_menu.menu)
        return;
    input_menu.ui = ui;
    Fl_Group *saved = Fl_Group::current();
    Fl_Group::current(nullptr);
    input_menu.menu = new Fl_Menu_Button(0, 0, 0, 0);
    Fl_Group::current(saved);
    input_menu.menu->box(FL_NO_BOX); // standalone popup, like the table menus
    input_menu.menu->user_data(&input_menu);
    input_menu.menu->add("Cut", FL_COMMAND + 'x', InputMenu::picked, (void *)1);
    input_menu.menu->add("Copy", FL_COMMAND + 'c', InputMenu::picked, (void *)2);
    input_menu.menu->add("Paste", FL_COMMAND + 'v', InputMenu::picked, (void *)3);
    input_menu.menu->add("Select All", FL_COMMAND + 'a', InputMenu::picked, (void *)4);
    // Parent it to the window so the theme pass styles it like every other menu.
    ui->window->add(input_menu.menu);
    input_menu.menu->hide();
}

void InputMenu::picked(Fl_Widget *w, void *data)
{
    InputMenu *self = (InputMenu *)w->user_data();
    Fl_Input *in = dynamic_cast<Fl_Input *>(Fl::focus());
    if (!self || !in)
        return;
    switch ((intptr_t)data) {
    case 1: in->cut(); break;
    case 2: in->copy(1); break;
    case 3: Fl::paste(*in, 1); break;
    case 4: in->insert_position(0, in->size()); break; // select all (1.4 API)
    }
    in->redraw();
}

int TextInput::handle(int event)
{
    InputMenu &input_menu = shared_input_menu();
    if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE && !readonly() && input_menu.menu) {
        if (Fl::focus() != this)
            take_focus();
        // The popup is a separate window, so give it the active theme before showing.
        if (input_menu.ui) {
            dbg_ui_t *u = input_menu.ui;
            input_menu.menu->color(u->color("surface"));
            input_menu.menu->textcolor(u->color("text"));
            input_menu.menu->labelcolor(u->color("text"));
            input_menu.menu->selection_color(u->color("selection"));
            input_menu.menu->textfont(u->ui_font);
            input_menu.menu->textsize(u->theme.font_size);
        }
        input_menu.menu->popup();
        return 1;
    }
    return Fl_Input::handle(event);
}
