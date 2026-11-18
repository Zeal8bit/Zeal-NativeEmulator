/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Sizes of the debugger layout and the Video window, saved in the
 * configuration under FLTK_* keys. */
#pragma once

namespace zui::debugger {

/** Debugger layout saved under FLTK_* keys (Nuklear's panel keys are left
 *  untouched; their meaning differs). 0 = default. */
struct LayoutConfig {
    int left_w = 0;
    int screen_h = 0;
    int right_w = 0;
    int stack_h = 0;          /* Stack height in the right column */
    bool cpu_more = false;    /* CPU panel shows interrupts, (HL)/(SP), timing */
    bool cpu_collapsed = false;
    bool stack_collapsed = false;
    bool lists_collapsed = false; /* Watch / Breakpoints */
    int lists_tab = 0;            /* 0 Watch, 1 Breakpoints */
    bool memory_collapsed = false;
    int video_x = -1;
    int video_y = -1;
    int video_w = 0;
    int video_h = 0;
    bool video_shown = false;
};
extern LayoutConfig g_layout;

/** Read g_layout from the configuration and save it with it. */
void layout_config_load();

} // namespace zui::debugger
