/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "video/inspector.h"
#include "video/sprite_data.h"
#include "video/sprite_inspector.h"
#include "video/sprite_sheet.h"
#include "video/vram_image.h"

namespace zui::video {

class VideoWindow : public Fl_Double_Window {
public:
    VideoWindow();
    void resize(int X, int Y, int W, int H) override;
    void draw() override;
    void refresh(bool paused, bool force);
    void set_view(int view);

private:
    static void cb_view(Fl_Widget* w, void* data);
    static void cb_zoom(Fl_Widget*, void* data);
    static void cb_close(Fl_Widget*, void*);

    int m_view = DBG_TILEMAP_LAYER0;
    double m_last = 0;
    Fl_Choice* m_zoom;
    Fl_Scroll* m_scroll;
    VramImage* m_image;
    Inspector* m_inspector;
    SpriteData m_sprites;
    SpriteSheet* m_sheet;
    SpriteInspector* m_sprite_inspector;
};

} // namespace zui::video
