/**
 * ===========================================================
 *                  ZEAL VIEW
 * ===========================================================
 */

#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"

#define PANEL_FLAGS ( NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MOVABLE )

void ui_panel_display(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    (void)panel; // unreferenced
    (void)dbg; // unreferenced

    struct nk_context* ctx = dctx->ctx;
    struct nk_image* img = &dctx->view;

    int height = panel->rect.h - NK_WIDGET_TITLE_HEIGHT; // account for window title

    nk_layout_row_dynamic(ctx, height, 1);
    nk_image(ctx, *img);
}
