/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "frontend/common/panels.h"
#include "utils/log.h"
config_panel_t config_panels[FRONTEND_PANEL_COUNT] = {
    { .key = "P_VIDEO", .title = "Video", .hidden = false },
    { .key = "P_BREAKPOINTS", .title = "Breakpoints", .hidden = false },
    { .key = "P_CPU", .title = "CPU", .hidden = false },
    { .key = "P_MEMORY", .title = "Memory Viewer", .hidden = false },
    { .key = "P_DISASSEMBLER", .title = "Disassembler", .hidden = false },
    { .key = "P_VRAM", .title = "VRAM viewer", .hidden = true },
    { .key = "P_MMU", .title = "MMU Viewer", .hidden = false },
    { .key = "P_SEMIHOST", .title = "Semihost", .hidden = false },
};

void config_panel_load(config_panel_t* panel)
{
    char key[80];
    sprintf(key, "%s_HIDDEN", panel->key);
    panel->hidden = config_get(key, panel->hidden);
    sprintf(key, "%s_MINIMIZED", panel->key);
    panel->minimized = config_get(key, 0);
    sprintf(key, "%s_POS_X", panel->key);
    panel->x = config_get(key, panel->x);
    sprintf(key, "%s_POS_Y", panel->key);
    panel->y = config_get(key, panel->y);
    sprintf(key, "%s_WIDTH", panel->key);
    panel->width = config_get(key, panel->width);
    sprintf(key, "%s_HEIGHT", panel->key);
    panel->height = config_get(key, panel->height);
    if (config.arguments.verbose > 2) {
        log_printf("[UI] === Panel: %s ===\n", panel->title);
        log_printf("[UI]     width: %d\n", panel->width);
        log_printf("[UI]    height: %d\n", panel->height);
        log_printf("[UI]         x: %d\n", panel->x);
        log_printf("[UI]         y: %d\n", panel->y);
        log_printf("[UI]    hidden: %d\n", panel->hidden);
        log_printf("[UI] minimized: %d\n", panel->minimized);
    }
}

void config_panels_save(rini_config* ini)
{
    char key[80];
    for (int i = 0; i < FRONTEND_PANEL_COUNT; i++) {
        config_panel_t* panel = &config_panels[i];
        rini_set_config_comment_line(ini, panel->title);
        if (panel->width < 1) config_panel_load(panel);
        sprintf(key, "%s_HIDDEN", panel->key);
        rini_set_config_value(ini, key, panel->hidden, "Enabled");
        sprintf(key, "%s_MINIMIZED", panel->key);
        rini_set_config_value(ini, key, panel->minimized, "Minimized");
        sprintf(key, "%s_POS_X", panel->key);
        rini_set_config_value(ini, key, panel->x, "X");
        sprintf(key, "%s_POS_Y", panel->key);
        rini_set_config_value(ini, key, panel->y, "Y");
        sprintf(key, "%s_WIDTH", panel->key);
        rini_set_config_value(ini, key, panel->width, "Width");
        sprintf(key, "%s_HEIGHT", panel->key);
        rini_set_config_value(ini, key, panel->height, "Height");
    }
}
