#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"

/**
 * ===========================================================
 *                  MENUBAR
 * ===========================================================
 */
#define PANEL_FLAGS (NK_WINDOW_NO_SCROLLBAR)
#define ROW_HEIGHT  30


void ui_menubar(struct dbg_ui_t* dctx, dbg_t* dbg, dbg_ui_panel_t *panels, int panels_size)
{
    struct nk_context* ctx = dctx->ctx;
    char buffer[60];

    if (nk_begin(ctx, "Menubar", nk_rect(0, 0, GetScreenWidth(), MENUBAR_HEIGHT), PANEL_FLAGS)) {

        nk_menubar_begin(ctx);
        /* menu #1 */
        nk_layout_row_begin(ctx, NK_STATIC, MENUBAR_HEIGHT, 5);
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "File", NK_TEXT_LEFT, nk_vec2(120, 200)))
        {
            nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);
            // if (nk_menu_item_label(ctx, "About", NK_TEXT_LEFT));
            if (nk_menu_item_label(ctx, "Quit", NK_TEXT_LEFT)) {
                dbg->running = false;
            }
            nk_menu_end(ctx);
        }
        /* menu #2 */
        nk_layout_row_push(ctx, 60);
        if (nk_menu_begin_label(ctx, "View", NK_TEXT_LEFT, nk_vec2(200, 600)))
        {
            nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);
            for(int i = 0; i < panels_size; i++) {
                dbg_ui_panel_t *panel = &panels[i];
                struct nk_window *win = nk_window_find(ctx, panel->title);

                sprintf(buffer, "Hide %s", panel->title);
                nk_bool hidden = (win->flags & NK_WINDOW_HIDDEN);
                nk_checkbox_label(ctx, buffer, &hidden);

                if(hidden) {
                    nk_window_show(ctx, panel->title, NK_HIDDEN);
                    panel->flags |= NK_WINDOW_HIDDEN;
                } else {
                    nk_window_show(ctx, panel->title, NK_SHOWN);
                    panel->flags &= ~NK_WINDOW_HIDDEN;
                }
            }

            if (nk_menu_item_label(ctx, "Reset Layout", NK_TEXT_LEFT)) {
                SetWindowSize(1280, 960);
                for(int i = 0; i < panels_size; i++) {
                    dbg_ui_panel_t *panel = &panels[i];
                    nk_window_set_bounds(ctx, panel->title, panel->rect_default);
                }
            }
            nk_menu_end(ctx);
        }
        nk_menubar_end(ctx);
    }
    nk_end(ctx);
}
