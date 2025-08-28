/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * ===========================================================
 *                  MMU VIEWER
 * ===========================================================
 */

#include <stdio.h>
#include <ctype.h>
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"
#include "debugger/zeal_debugger.h"


void ui_panel_mmu(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    dbg_mmu_t info;
    char buf[32];
    struct nk_context* ctx = dctx->ctx;

    int ok = debugger_custom(dbg, ZEAL_DBG_OP_GET_MMU, &info);
    if (!ok) {
        nk_label(ctx, "Could not get MMU information", NK_TEXT_LEFT);
        return;
    }

    nk_layout_row_dynamic(ctx, 25, 3);
    nk_label(ctx, "Virt Addr", NK_TEXT_CENTERED);
    nk_label(ctx, "Phys Addr", NK_TEXT_CENTERED);
    nk_label(ctx, "Device", NK_TEXT_CENTERED);

    for (int i = 0; i < ZEAL_DBG_MMU_PAGES; i++) {
        dbg_mmu_entry_t* e = &info.entries[i];

        snprintf(buf, sizeof(buf), "0x%04X", e->virt_addr);
        nk_label(ctx, buf, NK_TEXT_CENTERED);
        snprintf(buf, sizeof(buf), "0x%08X", e->phys_addr);
        nk_label(ctx, buf, NK_TEXT_CENTERED);
        if (e->device && e->device[0]) {
            nk_label(ctx, e->device, NK_TEXT_LEFT);
        } else {
            nk_label(ctx, "Unmapped", NK_TEXT_LEFT);
        }
    }
    (void)panel;
}

