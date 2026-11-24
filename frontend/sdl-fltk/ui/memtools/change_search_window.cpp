/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "memtools/change_search_window.h"
#include <cstdint>
#include <cstdio>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "app/main_window.h"
#include "memtools/candidate_list.h"
#include "memtools/change_search.h"
#include "memtools/pattern.h"
#include "memtools/snapshot_source.h"
#include "theme/style.h"
#include "theme/theme.h"

namespace zui::memtools {

using namespace zui::app;
using namespace zui::theme;

static ChangeSearchWindow* s_change_window;

ChangeSearchWindow::ChangeSearchWindow()
    : Fl_Double_Window(W, H, "Changed Bytes")
{
    color(COLOR_SURFACE);
    int y = HEADER_H + 6;
    m_filter = new Fl_Choice(8, y, 132, BAR_H - 8);
    m_filter->add("Changed|Unchanged|Increased|Decreased|Equal to");
    m_filter->value(0);
    m_filter->textsize(12);
    m_filter->clear_visible_focus();
    m_filter->tooltip("Compare each candidate's byte at this pause with the previous pause");
    m_filter->callback(cb_filter, this);
    m_value = new Fl_Input(146, y, 60, BAR_H - 8);
    style_input(m_value);
    m_value->value("00");
    m_value->tooltip("Byte value: hex (3E, $3E) or decimal with # (#62)");
    m_apply = new Fl_Button(212, y, 76, BAR_H - 8, "Apply");
    style_button(m_apply);
    m_apply->labelcolor(COLOR_AMBER);
    m_apply->tooltip("Keep only the candidates that pass the filter");
    m_apply->callback(cb_apply, this);
    m_reset = new Fl_Button(294, y, 78, BAR_H - 8, "Reset");
    style_button(m_reset);
    m_reset->tooltip("Forget the candidates and start over with all 65536 bytes");
    m_reset->callback(cb_reset, this);
    y += BAR_H - 4;
    m_auto = new Fl_Check_Button(8, y, W - 16, BAR_H - 8, "Narrow with this filter at every pause");
    style_check(m_auto);
    y += BAR_H - 6;
    m_status = new Fl_Box(8, y, W - 16, 20);
    m_status->labelsize(12);
    m_status->labelcolor(COLOR_DIM);
    m_status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    y += 24;
    m_bar = new Fl_Scrollbar(W - 14, y, 14, H - y);
    m_bar->type(FL_VERTICAL);
    m_bar->linesize(1);
    m_bar->color(COLOR_SURFACE);
    m_bar->callback(cb_bar, this);
    m_list = new CandidateList(0, y, W - 14, H - y, m_bar);
    m_list->source = &m_source;
    resizable(m_list);
    end();
    size_range(320, 240);
    set_non_modal();
    update_filter();
    update();
}

void ChangeSearchWindow::paused()
{
    if (m_search.active() && m_auto->value()) {
        apply();
    } else {
        update();
    }
}

void ChangeSearchWindow::draw()
{
    Fl_Double_Window::draw();
    char extra[64] = "";
    if (m_search.active()) {
        snprintf(extra, sizeof(extra), "ROUND %d  %zu LEFT", m_search.rounds(), m_search.candidates().size());
    }
    draw_header(0, 0, w(), "CHANGED BYTES", extra);
}

void ChangeSearchWindow::resize(int X, int Y, int W_, int H_)
{
    Fl_Double_Window::resize(X, Y, W_, H_);
    m_bar->resize(W_ - 14, m_list->y(), 14, H_ - m_list->y());
    m_list->resize(0, m_list->y(), W_ - 14, H_ - m_list->y());
}

void ChangeSearchWindow::apply()
{
    const uint8_t* prev = nullptr;
    const uint8_t* cur = nullptr;
    if (m_source.get == nullptr || !m_source.get(&prev, &cur)) {
        status("No pause snapshot yet: pause the machine first.", COLOR_BREAK);
        return;
    }
    const Filter filter = (Filter) m_filter->value();
    uint8_t value = 0;
    if (filter == Filter::Equal && !parse_byte(m_value->value(), &value)) {
        status("Value: hex byte (3E) or decimal with # (#62)", COLOR_BREAK);
        return;
    }
    if (filter != Filter::Equal && prev == nullptr) {
        status("Needs two pauses: continue, then pause again.", COLOR_BREAK);
        return;
    }
    m_search.narrow(filter, value, prev, cur);
    m_list->scroll_to(0);
    update();
}

void ChangeSearchWindow::update()
{
    if (!m_search.active()) {
        status("Starts with all 65536 bytes; each Apply narrows the list.", COLOR_DIM);
        m_list->items = nullptr;
    } else {
        char t[96];
        snprintf(t, sizeof(t), "Round %d: %zu candidate%s. Click one to view it.", m_search.rounds(),
                 m_search.candidates().size(), m_search.candidates().size() == 1 ? "" : "s");
        status(t, COLOR_DIM);
        m_list->items = &m_search.candidates();
    }
    m_list->sync();
    redraw();
}

void ChangeSearchWindow::update_filter()
{
    if ((Filter) m_filter->value() == Filter::Equal) {
        m_value->activate();
    } else {
        m_value->deactivate();
    }
}

void ChangeSearchWindow::status(const char* text, Fl_Color c)
{
    m_status->copy_label(text);
    m_status->labelcolor(c);
    m_status->redraw();
}

void ChangeSearchWindow::cb_filter(Fl_Widget*, void* d)
{ static_cast<ChangeSearchWindow*>(d)->update_filter(); }

void ChangeSearchWindow::cb_apply(Fl_Widget*, void* d)
{ static_cast<ChangeSearchWindow*>(d)->apply(); }

void ChangeSearchWindow::cb_bar(Fl_Widget*, void* d)
{
    ChangeSearchWindow* self = static_cast<ChangeSearchWindow*>(d);
    self->m_list->scroll_to(self->m_bar->value());
}

void ChangeSearchWindow::cb_reset(Fl_Widget*, void* d)
{
    ChangeSearchWindow* self = static_cast<ChangeSearchWindow*>(d);
    self->m_search.stop();
    self->update();
}

void change_search_show(const SnapshotSource& source)
{
    if (s_change_window == nullptr) {
        s_change_window = new ChangeSearchWindow();
        s_change_window->source(source);
        MainWindow* mw = main_window();
        if (mw && mw->shown()) {
            s_change_window->position(mw->x() + 40, mw->y() + 80);
        }
    }
    s_change_window->show();
}

void change_search_paused()
{
    if (s_change_window) {
        s_change_window->paused();
    }
}

void change_search_destroy()
{
    delete s_change_window;
    s_change_window = nullptr;
}

} // namespace zui::memtools
