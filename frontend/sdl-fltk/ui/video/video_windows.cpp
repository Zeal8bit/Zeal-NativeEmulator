/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "video/video_windows.h"
#include "common/machine.h"
#include "debugger/layout_config.h"
#include "video/video_window.h"

namespace zui::video {

using namespace zui::debugger;

static VideoWindow* s_window;

void video_window_show(bool show)
{
    if (!show) {
        if (s_window && s_window->shown()) {
            video_window_capture_config();
            s_window->hide();
        }
        g_layout.video_shown = false;
        return;
    }
    if (s_window == nullptr) {
        s_window = new VideoWindow();
        const LayoutConfig& c = g_layout;
        if (c.video_w > 200 && c.video_h > 150) {
            s_window->resize(c.video_x >= 0 ? c.video_x : s_window->x(),
                             c.video_y >= 0 ? c.video_y : s_window->y(), c.video_w, c.video_h);
        }
    }
    g_layout.video_shown = true;
    s_window->show();
    s_window->refresh(fz_paused(), true);
}

void video_window_suspend()
{
    if (s_window && s_window->shown()) {
        video_window_capture_config();
        s_window->hide();
    }
}

bool video_window_shown()
{
    return s_window && s_window->shown();
}

void video_window_refresh(bool paused, bool force)
{
    if (s_window) {
        s_window->refresh(paused, force);
    }
}

void video_window_capture_config()
{
    if (s_window == nullptr || !s_window->shown()) {
        return;
    }
    g_layout.video_x = s_window->x();
    g_layout.video_y = s_window->y();
    g_layout.video_w = s_window->w();
    g_layout.video_h = s_window->h();
}

void video_window_destroy()
{
    if (s_window) {
        video_window_capture_config();
        s_window->hide();
        delete s_window;
        s_window = nullptr;
    }
}

} // namespace zui::video
