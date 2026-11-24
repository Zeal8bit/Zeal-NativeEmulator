/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "memtools/form_dialog.h"
#include <cstddef>
#include <functional>
#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include "app/main_window.h"
#include "theme/style.h"
#include "theme/theme.h"

namespace zui::memtools {

using namespace zui::app;
using namespace zui::theme;

static void place_over_main(Fl_Window* win)
{
    MainWindow* mw = main_window();
    if (mw && mw->shown()) {
        win->position(mw->x() + (mw->w() - win->w()) / 2, mw->y() + (mw->h() - win->h()) / 3);
    } else {
        win->hotspot(win);
    }
}

static void select_all(Fl_Input* in)
{
#if FL_API_VERSION >= 10400
    in->insert_position(in->size(), 0);
#else
    in->position(in->size(), 0);
#endif
}

FormDialog::FormDialog(const char* title, const char* ok_label, const std::vector<const char*>& labels, const char* note)
    : Fl_Double_Window(W, 100, title), m_title(title)
{
    color(COLOR_SURFACE);
    int y = HEADER_H + 12;
    for (const char* label : labels) {
        Fl_Box* l = new Fl_Box(12, y, LABEL_W, ROW_H - 6, label);
        l->labelsize(12);
        l->labelcolor(COLOR_AMBER);
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Input* in = new Fl_Input(12 + LABEL_W, y, W - 24 - LABEL_W, ROW_H - 6);
        style_input(in);
        m_inputs.push_back(in);
        y += ROW_H;
    }
    m_note = new Fl_Box(12, y, W - 24, 34);
    m_note->labelsize(12);
    m_note->labelcolor(COLOR_DIM);
    m_note->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP | FL_ALIGN_TOP);
    m_note->copy_label(note ? note : "");
    y += 34;
    m_message = new Fl_Box(12, y, W - 24, 34);
    m_message->labelsize(12);
    m_message->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP | FL_ALIGN_TOP);
    y += 38;
    Fl_Button* cancel = new Fl_Button(W - 12 - 2 * 92 - 8, y, 92, 26, "Cancel");
    style_button(cancel);
    cancel->callback(cb_cancel, this);
    Fl_Button* ok = new Fl_Button(W - 12 - 92, y, 92, 26, ok_label);
    style_button(ok);
    ok->color(COLOR_SELECT);
    ok->labelcolor(COLOR_AMBER);
    ok->shortcut(FL_Enter);
    ok->callback(cb_ok, this);
    end();
    size(W, y + 26 + 12);
    set_modal();
    callback(cb_cancel, this);
}

Fl_Input* FormDialog::input(int i) const
{ return m_inputs[(size_t) i]; }

const char* FormDialog::value(int i) const
{ return m_inputs[(size_t) i]->value(); }

void FormDialog::value(int i, const char* text)
{ m_inputs[(size_t) i]->value(text); }

void FormDialog::note(const char* text)
{ m_note->copy_label(text); redraw(); }

void FormDialog::error(const char* text)
{ message(text, COLOR_BREAK); }

void FormDialog::warn(const char* text)
{ message(text, COLOR_AMBER); }

bool FormDialog::run(const std::function<bool(FormDialog&)>& accept)
{
    place_over_main(this);
    show();
    if (!m_inputs.empty()) {
        m_inputs[0]->take_focus();
        select_all(m_inputs[0]);
    }
    m_accept = &accept;
    m_result = false;
    while (shown()) Fl::wait();
    m_accept = nullptr;
    return m_result;
}

void FormDialog::draw()
{
    Fl_Double_Window::draw();
    draw_header(0, 0, w(), m_title);
}

void FormDialog::message(const char* text, Fl_Color c)
{
    m_message->labelcolor(c);
    m_message->copy_label(text);
    redraw();
}

void FormDialog::cb_ok(Fl_Widget*, void* data)
{
    FormDialog* self = static_cast<FormDialog*>(data);
    if (self->m_accept && !(*self->m_accept)(*self)) {
        return;
    }
    self->m_result = true;
    self->hide();
}

void FormDialog::cb_cancel(Fl_Widget*, void* data)
{
    FormDialog* self = static_cast<FormDialog*>(data);
    self->m_result = false;
    self->hide();
}

} // namespace zui::memtools
