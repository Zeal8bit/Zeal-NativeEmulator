/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/layout_config.h"
#include "common/machine.h"
#include "common/persist.h"

namespace zui::debugger {

using namespace zui::common;

LayoutConfig g_layout;

namespace {

void layout_save()
{
    const LayoutConfig& c = g_layout;
    set_preference("debugger", "left_width", c.left_w);
    set_preference("debugger", "screen_height", c.screen_h);
    set_preference("debugger", "right_width", c.right_w);
    set_preference("debugger", "stack_height", c.stack_h);
    set_preference("debugger", "cpu_more", c.cpu_more);
    set_preference("debugger", "cpu_collapsed", c.cpu_collapsed);
    set_preference("debugger", "stack_collapsed", c.stack_collapsed);
    set_preference("debugger", "lists_collapsed", c.lists_collapsed);
    set_preference("debugger", "lists_tab", c.lists_tab);
    set_preference("debugger", "memory_collapsed", c.memory_collapsed);
    set_preference("video_window", "x", c.video_x);
    set_preference("video_window", "y", c.video_y);
    set_preference("video_window", "width", c.video_w);
    set_preference("video_window", "height", c.video_h);
    set_preference("video_window", "shown", c.video_shown);
}

} // namespace

void layout_config_load()
{
    persist_register(layout_save);
    g_layout.left_w = preference("debugger", "left_width", 0);
    g_layout.screen_h = preference("debugger", "screen_height", 0);
    g_layout.right_w = preference("debugger", "right_width", 0);
    g_layout.stack_h = preference("debugger", "stack_height", 0);
    g_layout.cpu_more = preference("debugger", "cpu_more", false);
    g_layout.cpu_collapsed = preference("debugger", "cpu_collapsed", false);
    g_layout.stack_collapsed = preference("debugger", "stack_collapsed", false);
    g_layout.lists_collapsed = preference("debugger", "lists_collapsed", false);
    g_layout.lists_tab = preference("debugger", "lists_tab", 0);
    g_layout.memory_collapsed = preference("debugger", "memory_collapsed", false);
    g_layout.video_x = preference("video_window", "x", -1);
    g_layout.video_y = preference("video_window", "y", -1);
    g_layout.video_w = preference("video_window", "width", 0);
    g_layout.video_h = preference("video_window", "height", 0);
    g_layout.video_shown = preference("video_window", "shown", false);
}

} // namespace zui::debugger
