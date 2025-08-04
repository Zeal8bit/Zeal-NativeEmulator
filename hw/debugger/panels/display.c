/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * ===========================================================
 *                  ZEAL VIEW
 * ===========================================================
 */

#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"

#define PANEL_FLAGS ( NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MOVABLE )

/* Color of the title bar when the CPU is paused  */
#define PAUSED_BORDER   nk_rgb(0x8e, 0x15, 0x15)

void ui_panel_display(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    struct nk_context* ctx = dctx->ctx;
    struct nk_image* img = &dctx->view;

    int height = panel->rect.h - NK_WIDGET_TITLE_HEIGHT; // account for window title

    const bool paused = debugger_is_paused(dbg);
    if (paused) {
        panel->title = "Video (paused)";
        panel->override_header_style = true;
        panel->header_style = nk_style_item_color(PAUSED_BORDER);
    } else {
        panel->title = "Video";
        panel->override_header_style = false;
    }

    nk_layout_row_dynamic(ctx, height, 1);
    nk_image(ctx, *img);
}
