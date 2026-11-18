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

void layout_save(rini_config* ini)
{
    const LayoutConfig& c = g_layout;
    rini_set_config_comment_line(ini, "SDL/FLTK debugger layout");
    rini_set_config_value(ini, "FLTK_DBG_LEFT_WIDTH", c.left_w, "Screen/memory column width");
    rini_set_config_value(ini, "FLTK_DBG_SCREEN_HEIGHT", c.screen_h, "Screen height");
    rini_set_config_value(ini, "FLTK_DBG_RIGHT_WIDTH", c.right_w, "CPU/stack column width");
    rini_set_config_value(ini, "FLTK_DBG_STACK_HEIGHT", c.stack_h, "Stack height");
    rini_set_config_value(ini, "FLTK_DBG_CPU_MORE", c.cpu_more, "CPU panel: interrupts, (HL)/(SP), timing");
    rini_set_config_value(ini, "FLTK_DBG_CPU_COLLAPSED", c.cpu_collapsed, "CPU panel collapsed");
    rini_set_config_value(ini, "FLTK_DBG_STACK_COLLAPSED", c.stack_collapsed, "Stack panel collapsed");
    rini_set_config_value(ini, "FLTK_DBG_LISTS_COLLAPSED", c.lists_collapsed, "Watch/Breakpoints collapsed");
    rini_set_config_value(ini, "FLTK_DBG_LISTS_TAB", c.lists_tab, "Watch/Breakpoints tab (0 Watch, 1 Breakpoints)");
    rini_set_config_value(ini, "FLTK_DBG_MEMORY_COLLAPSED", c.memory_collapsed, "Memory panel collapsed");
    rini_set_config_value(ini, "FLTK_VIDEO_X", c.video_x, "Video window X");
    rini_set_config_value(ini, "FLTK_VIDEO_Y", c.video_y, "Video window Y");
    rini_set_config_value(ini, "FLTK_VIDEO_WIDTH", c.video_w, "Video window width");
    rini_set_config_value(ini, "FLTK_VIDEO_HEIGHT", c.video_h, "Video window height");
    rini_set_config_value(ini, "FLTK_VIDEO_SHOWN", c.video_shown, "Video window open");
}

} // namespace

void layout_config_load()
{
    persist_register(layout_save);
    g_layout.left_w = config_get("FLTK_DBG_LEFT_WIDTH", 0);
    g_layout.screen_h = config_get("FLTK_DBG_SCREEN_HEIGHT", 0);
    g_layout.right_w = config_get("FLTK_DBG_RIGHT_WIDTH", 0);
    g_layout.stack_h = config_get("FLTK_DBG_STACK_HEIGHT", 0);
    g_layout.cpu_more = config_get("FLTK_DBG_CPU_MORE", 0) != 0;
    g_layout.cpu_collapsed = config_get("FLTK_DBG_CPU_COLLAPSED", 0) != 0;
    g_layout.stack_collapsed = config_get("FLTK_DBG_STACK_COLLAPSED", 0) != 0;
    g_layout.lists_collapsed = config_get("FLTK_DBG_LISTS_COLLAPSED", 0) != 0;
    g_layout.lists_tab = config_get("FLTK_DBG_LISTS_TAB", 0);
    g_layout.memory_collapsed = config_get("FLTK_DBG_MEMORY_COLLAPSED", 0) != 0;
    g_layout.video_x = config_get("FLTK_VIDEO_X", -1);
    g_layout.video_y = config_get("FLTK_VIDEO_Y", -1);
    g_layout.video_w = config_get("FLTK_VIDEO_WIDTH", 0);
    g_layout.video_h = config_get("FLTK_VIDEO_HEIGHT", 0);
    g_layout.video_shown = config_get("FLTK_VIDEO_SHOWN", 0) != 0;
}

} // namespace zui::debugger
