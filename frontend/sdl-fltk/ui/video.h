/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* "Video" tool window: VRAM and sprite viewers. */
#pragma once

#include "debugger/layout_config.h"

namespace zui {

using namespace zui::debugger;


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
