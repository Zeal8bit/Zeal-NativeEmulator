/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "utils/config.h"
#define FRONTEND_PANEL_COUNT 8

typedef struct {
    const char* key;
    const char* title;
    bool hidden;
    bool minimized;
    int x;
    int y;
    int width;
    int height;
} config_panel_t;
extern config_panel_t config_panels[FRONTEND_PANEL_COUNT];
void config_panel_load(config_panel_t*);
void config_panels_save(rini_config*);
