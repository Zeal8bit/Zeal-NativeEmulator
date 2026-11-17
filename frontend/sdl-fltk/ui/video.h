/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* "Video & Profiler" tool window: VRAM viewers and semihost counters. */
#pragma once

namespace zui {

/** Debugger layout saved under FLTK_* keys (Nuklear's panel keys are left
 *  untouched; their meaning differs). 0 = default. */
struct LayoutConfig {
    int left_w = 0;
    int screen_h = 0;
    int right_w = 0;
    int breakpoints_h = 0;
    int video_x = -1;
    int video_y = -1;
    int video_w = 0;
    int video_h = 0;
    bool video_shown = false;
};
extern LayoutConfig g_layout;

void video_window_show(bool show);
bool video_window_shown();
/** Hide the window but remember it was open (leaving the debugger). */
void video_window_suspend();
/** Refresh the visible view (throttled internally while running). */
void video_window_refresh(bool paused, bool force);
/** Save the window geometry into g_layout. */
void video_window_capture_config();
void video_window_destroy();

} // namespace zui
