/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#define DBG_UI_MEM_TABS_MAX_COUNT  16

/* Each tab has its own address range and bytes count */
typedef struct {
    int addr;
    int size;
    int size_index;
    uint8_t* mem;
} dbg_ui_mem_tab_t;


typedef struct {
    dbg_ui_mem_tab_t tabs[DBG_UI_MEM_TABS_MAX_COUNT];
    int tabs_count;
    int current_tab;
} dbg_ui_mem_t;
