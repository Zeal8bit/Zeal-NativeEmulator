/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "console/console_view.h"
#include <cstdio>
#include <string>
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "common/persist.h"
#include "console/console_format.h"
#include "console/console_text.h"
#include "frontend/sdl-fltk/bridge/console.h"
#include "theme/style.h"
#include "theme/theme.h"
#include "tools/tool_registry.h"
#include "widgets/panel.h"

namespace zui::console {

using namespace zui::common;
using namespace zui::theme;
using namespace zui::tools;
using namespace zui::widgets;

namespace {

widgets::Panel* make_console(int x, int y, int w, int h)
{
    return new ConsoleView(x, y, w, h);
}

tools::ToolViewRegistrar s_reg(tools::TOOL_HARDWARE, 50, "Console", make_console);

} // namespace

ConsoleView::ConsoleView(int X, int Y, int W, int H)
    : Panel(X, Y, W, H, "UART CONSOLE")
{
    m_follow = new Fl_Check_Button(X, Y, 10, 10, "Follow");
    style_check(m_follow);
    m_follow->tooltip("Keep the newest output in view");
    m_follow->callback(cb_follow, this);
    m_time = new Fl_Check_Button(X, Y, 10, 10, "Time");
    style_check(m_time);
    m_time->tooltip("Show the emulated time (seconds) at which each line started");
    m_time->callback(cb_time, this);
    m_clear = new Fl_Button(X, Y, 10, 10, "Clear");
    style_button(m_clear);
    m_clear->callback(cb_clear, this);
    m_copy = new Fl_Button(X, Y, 10, 10, "Copy All");
    style_button(m_copy);
    m_copy->callback(cb_copy, this);
    m_save = new Fl_Button(X, Y, 10, 10, "Save...");
    style_button(m_save);
    m_save->callback(cb_save, this);
    m_bar = new Fl_Scrollbar(X, Y, 10, 10);
    m_bar->type(FL_VERTICAL);
    m_bar->linesize(1);
    m_bar->color(COLOR_SURFACE);
    m_bar->callback(cb_bar, this);
    m_text = new ConsoleText(X, Y, 10, 10, m_bar);
    m_text->tooltip("Click: select a line    Right-click: copy, clear, save");
    m_text->follow = preference("console", "follow", true);
    m_text->time = preference("console", "timestamps", false);
    m_text->on_follow = [this](bool f) { m_follow->value(f ? 1 : 0); };
    m_text->on_menu = [this] { context_menu(); };
    m_follow->value(m_text->follow);
    m_time->value(m_text->time);
    resizable(m_text);
    end();
    resize(X, Y, W, H);

    persist_register([this]() {
        set_preference("console", "follow", m_text->follow);
        set_preference("console", "timestamps", m_text->time);
    });
    Fl::add_timeout(0.1, cb_tick, this);
}

ConsoleView::~ConsoleView()
{
    Fl::remove_timeout(cb_tick, this);
}

void ConsoleView::resize(int X, int Y, int W, int H)
{
    Fl_Widget::resize(X, Y, W, H);
    const int by = Y + HEADER_H + 4;
    const int bh = BAR_H - 8;
    m_follow->resize(X + 8, by, 70, bh);
    m_time->resize(X + 80, by, 60, bh);
    m_clear->resize(X + 150, by, 56, bh);
    m_copy->resize(X + 210, by, 72, bh);
    m_save->resize(X + 286, by, 64, bh);
    const int ty = Y + HEADER_H + BAR_H;
    m_bar->resize(X + W - 14, ty, 14, H - (ty - Y));
    m_text->resize(X, ty, W - 14, H - (ty - Y));
    m_text->sync();
}

void ConsoleView::refresh(bool paused, bool force)
{
    (void) paused;
    if (force || fz_console_serial() != m_serial) update();
}

void ConsoleView::update()
{
    m_serial = fz_console_serial();
    char text[64];
    snprintf(text, sizeof(text), "%d lines  %llu bytes", fz_console_count(),
             (unsigned long long) fz_console_bytes());
    extra(text);
    m_text->sync();
    redraw();
}

void ConsoleView::copy_all()
{
    const std::string s = all_text(m_text->time);
    Fl::copy(s.c_str(), (int) s.size(), 1);
    notif_show("Copied %d console lines", fz_console_count());
}

void ConsoleView::clear()
{
    fz_console_clear();
    m_text->cleared();
    update();
}

void ConsoleView::save()
{
    Fl_Native_File_Chooser chooser;
    chooser.title("Save UART Console");
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    chooser.filter("Text\t*.{txt,log}");
    chooser.preset_file("uart.log");
    if (chooser.show() != 0 || chooser.filename() == nullptr) return;
    FILE* f = fl_fopen(chooser.filename(), "wb");
    if (f == nullptr) {
        notif_show("Could not write %s", chooser.filename());
        return;
    }
    const std::string s = all_text(m_text->time);
    const bool ok = fwrite(s.data(), 1, s.size(), f) == s.size();
    fclose(f);
    notif_show(ok ? "Console saved to %s" : "Could not write %s", chooser.filename());
}

void ConsoleView::context_menu()
{
    fz_console_line_t line;
    const bool have_line = m_text->selected_line(&line);
    Fl_Menu_Item items[] = {
        { "Copy Line", 0, nullptr, nullptr, have_line ? 0 : FL_MENU_INACTIVE, 0, 0, 0, 0 },
        { "Copy All", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Follow Output", 0, nullptr, nullptr, FL_MENU_TOGGLE | (m_text->follow ? FL_MENU_VALUE : 0), 0, 0, 0, 0 },
        { "Show Timestamps", 0, nullptr, nullptr, FL_MENU_TOGGLE | (m_text->time ? FL_MENU_VALUE : 0) | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        { "Clear", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { "Save...", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
    };
    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
    if (picked == nullptr) return;
    switch ((int) (picked - items)) {
        case 0: {
            const std::string s = line_text(line, m_text->time);
            Fl::copy(s.c_str(), (int) s.size(), 1);
            break;
        }
        case 1: copy_all(); break;
        case 2: set_follow(!m_text->follow); break;
        case 3: set_time(!m_text->time); break;
        case 4: clear(); break;
        case 5: save(); break;
        default: break;
    }
}

void ConsoleView::set_follow(bool f)
{
    m_text->follow = f;
    m_follow->value(f ? 1 : 0);
    update();
}

void ConsoleView::set_time(bool t)
{
    m_text->time = t;
    m_time->value(t ? 1 : 0);
    m_text->redraw();
}

void ConsoleView::cb_tick(void* data)
{
    ConsoleView* self = (ConsoleView*) data;
    /* Faster than the tool window refresh so output appears promptly */
    if (self->visible_r() && fz_console_serial() != self->m_serial) self->update();
    Fl::repeat_timeout(0.1, cb_tick, data);
}

void ConsoleView::cb_follow(Fl_Widget*, void* data)
{
    ConsoleView* self = (ConsoleView*) data;
    self->set_follow(self->m_follow->value() != 0);
}

void ConsoleView::cb_time(Fl_Widget*, void* data)
{
    ConsoleView* self = (ConsoleView*) data;
    self->set_time(self->m_time->value() != 0);
}

void ConsoleView::cb_clear(Fl_Widget*, void* data)
{ ((ConsoleView*) data)->clear(); }

void ConsoleView::cb_copy(Fl_Widget*, void* data)
{ ((ConsoleView*) data)->copy_all(); }

void ConsoleView::cb_save(Fl_Widget*, void* data)
{ ((ConsoleView*) data)->save(); }

void ConsoleView::cb_bar(Fl_Widget*, void* data)
{
    ConsoleView* self = (ConsoleView*) data;
    self->m_text->scroll_to(self->m_bar->value());
}

} // namespace zui::console
