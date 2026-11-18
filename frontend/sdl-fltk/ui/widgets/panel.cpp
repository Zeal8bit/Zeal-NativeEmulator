/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "widgets/panel.h"
#include <cstdio>
#include <cstring>
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/fl_draw.H>
#include "theme/theme.h"

namespace zui::widgets {

using namespace zui::theme;

namespace {

constexpr int CHEVRON_W = 14;
constexpr int BUTTON_W = 18;

} // namespace

Panel::Panel(int X, int Y, int W, int H, const char* title)
    : Fl_Group(X, Y, W, H), m_title(title)
{
    box(FL_FLAT_BOX);
    color(COLOR_SURFACE);
}

int Panel::header_h() const
{ return m_header ? HEADER_H : 0; }

int Panel::top() const
{ return y() + header_h(); }

void Panel::show_header(bool show)
{
    m_header = show;
    resize(x(), y(), w(), h());
}

void Panel::collapsible(bool on)
{
    m_collapsible = on;
    redraw();
}

void Panel::set_collapsed(bool collapsed)
{
    m_collapsed = collapsed;
    /* Only the header remains: children neither draw nor take events */
    for (int i = 0; i < children(); i++) {
        if (collapsed) child(i)->hide(); else child(i)->show();
    }
    redraw();
}

void Panel::maximized(bool on)
{
    m_maximized = on;
    damage(FL_DAMAGE_ALL, x(), y(), w(), HEADER_H);
}

void Panel::header_toggle(const char* label, bool on, std::function<void(bool)> changed)
{
    m_chip = label;
    m_chip_on = on;
    m_chip_changed = std::move(changed);
    damage(FL_DAMAGE_ALL, x(), y(), w(), HEADER_H);
}

void Panel::extra(const char* text)
{
    if (strcmp(m_extra, text) == 0) return;
    snprintf(m_extra, sizeof(m_extra), "%s", text);
    damage(FL_DAMAGE_ALL, x(), y(), w(), HEADER_H);
}

int Panel::title_x() const
{ return x() + (m_collapsible ? CHEVRON_W : 0); }

int Panel::maximize_x() const
{ return on_maximize ? x() + w() - BUTTON_W - 2 : x() + w(); }

void Panel::chip_rect(int& cx, int& cw) const
{
    fl_font(FL_HELVETICA_BOLD, 10);
    cw = m_chip ? (int) fl_width(m_chip) + 10 : 0;
    fl_font(DATA_FONT, DATA_SIZE);
    const int extra_w = m_extra[0] ? (int) fl_width(m_extra) + 12 : 0;
    cx = maximize_x() - 6 - extra_w - cw;
}

void Panel::draw_header_content(int, int, int)
{}

void Panel::draw_panel_header()
{
    if (!m_header) return;
    const int hx = title_x();
    draw_header(x(), y(), w(), "", nullptr);
    fl_font(DATA_FONT, DATA_SIZE);
    const int baseline = y() + HEADER_H - 1 - (HEADER_H - fl_height()) / 2 - fl_descent();
    fl_color(COLOR_AMBER);
    fl_draw(m_title, hx + 10, baseline);
    if (m_extra[0]) {
        /* Right-aligned, left of the maximise button */
        fl_color(COLOR_DIM);
        fl_draw(m_extra, maximize_x() - (int) fl_width(m_extra) - 8, baseline);
    }
    if (m_collapsible) {
        const int cx = x() + 9, cy = y() + HEADER_H / 2;
        fl_color(COLOR_DIM);
        if (m_collapsed) fl_polygon(cx - 2, cy - 4, cx + 3, cy, cx - 2, cy + 4);
        else fl_polygon(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
    }
    fl_font(DATA_FONT, DATA_SIZE);
    const int title_end = hx + 10 + (int) fl_width(m_title);
    int chip_x, chip_w;
    chip_rect(chip_x, chip_w);
    draw_header_content(title_end, y(), (m_chip ? chip_x : maximize_x()) - title_end);
    if (m_chip) {
        const int ch = HEADER_H - 10;
        const int cy = y() + 5;
        if (m_chip_on) {
            fl_color(COLOR_AMBER);
            fl_rectf(chip_x, cy, chip_w, ch);
            fl_color(COLOR_BG);
        } else {
            fl_color(COLOR_BORDER);
            fl_rect(chip_x, cy, chip_w, ch);
            fl_color(COLOR_DIM);
        }
        fl_font(FL_HELVETICA_BOLD, 10);
        fl_draw(m_chip, chip_x, cy, chip_w, ch, FL_ALIGN_CENTER);
    }
    if (on_maximize) {
        /* Square = maximise, two squares = restore */
        const int bx = maximize_x() + 4, by = y() + 7;
        fl_color(COLOR_DIM);
        if (m_maximized) {
            fl_rect(bx + 3, by, 8, 7);
            fl_rect(bx, by + 3, 8, 7);
        } else {
            fl_rect(bx, by, 10, 10);
            fl_xyline(bx, by + 2, bx + 9);
        }
    }
}

void Panel::draw()
{
    if (m_collapsed) {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
    } else {
        Fl_Group::draw();
    }
    draw_panel_header();
}

int Panel::handle(int event)
{
    if (m_header && event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE &&
        Fl::event_y() >= y() && Fl::event_y() < y() + HEADER_H) {
        const int ex = Fl::event_x();
        int chip_x, chip_w;
        chip_rect(chip_x, chip_w);
        if (m_chip && ex >= chip_x && ex < chip_x + chip_w) {
            m_chip_on = !m_chip_on;
            if (m_chip_changed) m_chip_changed(m_chip_on);
            damage(FL_DAMAGE_ALL, x(), y(), w(), HEADER_H);
            return 1;
        }
        if (on_maximize && ex >= maximize_x()) {
            on_maximize(this);
            return 1;
        }
        if (m_collapsible && on_collapse) {
            on_collapse(this);
            return 1;
        }
    }
    return Fl_Group::handle(event);
}

} // namespace zui::widgets
