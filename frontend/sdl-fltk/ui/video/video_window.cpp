/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "video/video_window.h"
#include <cstdint>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include "app/main_window.h"
#include "common/clock.h"
#include "common/machine.h"
#include "theme/theme.h"
#include "video/inspector.h"
#include "video/sprite_inspector.h"
#include "video/sprite_sheet.h"
#include "video/video_windows.h"
#include "video/vram_image.h"
#include "widgets/segment.h"

namespace zui::video {

using namespace zui::app;
using namespace zui::common;
using namespace zui::theme;
using namespace zui::widgets;

namespace {

enum { VIEW_SPRITES = DBG_VIEW_TOTAL, VIEW_COUNT };
constexpr int TOP_H = 38;
constexpr int INSPECTOR_W = 270;

static const char* s_view_names[] = { "Layer 0", "Layer 1", "Tileset", "Palette", "Font", "Sprites" };

} // namespace

VideoWindow::VideoWindow()
    : Fl_Double_Window(980, 660, FZ_WIN_NAME " - Video")
{
    color(COLOR_BG);
    int sx = 8;
    for (int i = 0; i < VIEW_COUNT; i++) {
        const int sw = 76;
        Segment* s = new Segment(sx, 6, sw, TOP_H - 10, s_view_names[i]);
        s->callback(cb_view, (void*) (intptr_t) i);
        if (i == 0) s->setonly();
        sx += sw + (i == DBG_FONT ? 14 : 2);
    }
    m_zoom = new Fl_Choice(w() - 90, 8, 80, TOP_H - 14);
    m_zoom->add("1x|2x|3x|4x");
    m_zoom->value(1);
    m_zoom->textsize(12);
    m_zoom->clear_visible_focus();
    m_zoom->callback(cb_zoom, this);
    m_zoom->tooltip("Zoom");

    m_scroll = new Fl_Scroll(0, TOP_H, w() - INSPECTOR_W - 4, h() - TOP_H);
    m_scroll->box(FL_FLAT_BOX);
    m_scroll->color(COLOR_BG);
    m_image = new VramImage(0, TOP_H, 10, 10);
    m_sheet = new SpriteSheet(0, TOP_H, 10, 10, &m_sprites);
    m_sheet->hide();
    m_scroll->end();
    m_inspector = new Inspector(w() - INSPECTOR_W, TOP_H, INSPECTOR_W, h() - TOP_H, m_image);
    m_image->inspector = m_inspector;
    m_sprite_inspector = new SpriteInspector(w() - INSPECTOR_W, TOP_H, INSPECTOR_W, h() - TOP_H, &m_sprites);
    m_sprite_inspector->hide();
    m_sheet->inspector = m_sprite_inspector;
    m_sprite_inspector->sheet = m_sheet;
    resizable(m_scroll);
    end();
    size_range(520, 320);
    /* A tool window: stays above the debugger, closes with it */
    set_non_modal();
    callback(cb_close);
}

void VideoWindow::resize(int X, int Y, int W, int H)
{
    Fl_Double_Window::resize(X, Y, W, H);
    m_zoom->resize(W - 90, 8, 80, TOP_H - 14);
    m_scroll->resize(0, TOP_H, W - INSPECTOR_W - 4, H - TOP_H);
    m_inspector->resize(W - INSPECTOR_W, TOP_H, INSPECTOR_W, H - TOP_H);
    m_sprite_inspector->resize(W - INSPECTOR_W, TOP_H, INSPECTOR_W, H - TOP_H);
}

void VideoWindow::draw()
{
    Fl_Double_Window::draw();
    fl_color(COLOR_BORDER);
    fl_xyline(0, TOP_H - 1, w());
}

void VideoWindow::refresh(bool paused, bool force)
{
    if (!shown()) return;
    const double now = now_seconds();
    if (!force && (paused || now - m_last < 0.25)) return;
    m_last = now;
    if (m_view == VIEW_SPRITES) {
        m_sprites.update();
        m_sheet->zoom = m_zoom->value() + 1;
        m_sheet->relayout();
        m_scroll->redraw();
        m_sprite_inspector->redraw();
    } else {
        m_image->update((dbg_vram_t) m_view, m_zoom->value() + 1);
        m_scroll->redraw();
        m_inspector->redraw();
    }
}

void VideoWindow::set_view(int view)
{
    m_view = view;
    const bool sprites = view == VIEW_SPRITES;
    {
        m_scroll->show();
        m_zoom->show();
        if (sprites) {
            m_image->hide();
            m_inspector->hide();
            m_sheet->show();
            m_sprite_inspector->show();
        } else {
            m_sheet->hide();
            m_sprite_inspector->hide();
            m_image->show();
            m_inspector->show();
        }
        m_scroll->scroll_to(0, 0);
    }
    refresh(false, true);
    redraw();
}

void VideoWindow::cb_view(Fl_Widget* w, void* data)
{
    static_cast<VideoWindow*>(w->window())->set_view((int) (intptr_t) data);
}

void VideoWindow::cb_zoom(Fl_Widget*, void* data)
{
    static_cast<VideoWindow*>(data)->refresh(false, true);
}

void VideoWindow::cb_close(Fl_Widget*, void*)
{
    video_window_show(false);
    if (main_window()) main_window()->update_menus();
}

} // namespace zui::video
