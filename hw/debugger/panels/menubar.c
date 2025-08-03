#include "hw/zeal.h"
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
    zeal_t* machine = (zeal_t*) (dbg->arg);
    char buffer[64];

    // menus will clip their height, so just allow them to be as tall as possible
    int windowHeight = GetScreenHeight();

    if (nk_begin(ctx, "Menubar", nk_rect(0, 0, GetScreenWidth(), MENUBAR_HEIGHT), PANEL_FLAGS)) {

        nk_menubar_begin(ctx);
        nk_layout_row_begin(ctx, NK_STATIC, MENUBAR_HEIGHT, 4);


        /* File Menu */
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "File", NK_TEXT_LEFT, nk_vec2(120, windowHeight)))
        {
            nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);

            if (nk_menu_item_label(ctx, "Debugger Off", NK_TEXT_LEFT)) {
                zeal_debug_disable(machine);
            }

            if (nk_menu_item_label(ctx, "Save Config", NK_TEXT_LEFT)) {
                config_window_update(true);
                config_save();
            }

            if (nk_menu_item_label(ctx, "Quit", NK_TEXT_LEFT)) {
                dbg->running = false;
            }
            nk_menu_end(ctx);
        }


        /* CPU Menu */
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "CPU", NK_TEXT_LEFT, nk_vec2(260, windowHeight)))
        {
            nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);
            if (nk_menu_item_label(ctx, "Pause                  Meta+F6", NK_TEXT_LEFT)) {
                debugger_pause(dbg);
            }
            if (nk_menu_item_label(ctx, "Continue               Meta+F5", NK_TEXT_LEFT)) {
                debugger_continue(dbg);
            }
            if (nk_menu_item_label(ctx, "Step Over             Meta+F10", NK_TEXT_LEFT)) {
                debugger_step_over(dbg);
            }
            if (nk_menu_item_label(ctx, "Step                  Meta+F11", NK_TEXT_LEFT)) {
                debugger_step(dbg);
            }
            if (nk_menu_item_label(ctx, "Toggle Breakpoint      Meta+F9", NK_TEXT_LEFT)) {
                debugger_toggle_breakpoint(dbg, machine->cpu.pc);
            }
            if (nk_menu_item_label(ctx, "Reset          Meta+Shift+Bksp", NK_TEXT_LEFT)) {
                debugger_reset(dbg);
            }
            nk_menu_end(ctx);
        }


        /* View Menu */
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "View", NK_TEXT_LEFT, nk_vec2(200, windowHeight)))
        {
            nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);
            for(int i = 0; i < panels_size; i++) {
                dbg_ui_panel_t *panel = &panels[i];

                snprintf(buffer, sizeof(buffer), "Hide %s", panel->title);
                if (nk_checkbox_label(ctx, buffer, &panel->hidden)) {
                    nk_window_show(ctx, panel->title, panel->hidden ? NK_HIDDEN : NK_SHOWN);
                }
            }

            nk_checkbox_label(ctx, "KB Passthru", &config.debugger.keyboard_passthru);

            nk_checkbox_label(ctx, "Hex Upper", &config.debugger.hex_upper);

            if (nk_menu_item_label(ctx, "Reset Layout", NK_TEXT_LEFT)) {
#ifndef PLATFORM_WEB
                SetWindowSize(1280, 960);
#endif
                for(int i = 0; i < panels_size; i++) {
                    dbg_ui_panel_t *panel = &panels[i];
                    panel->hidden = panel->hidden_default;
                    panel->flags &= ~(NK_WINDOW_MINIMIZED);
                    panel->rect = panel->rect_default;
                    nk_window_set_bounds(ctx, panel->title, panel->rect_default);
                    nk_window_show(ctx, panel->title, NK_SHOWN);
                    nk_window_collapse(ctx, panel->title, NK_MAXIMIZED);
                }
            }
            nk_menu_end(ctx);
        }


        /* CPU Menu */
        nk_layout_row_push(ctx, 60);
        if (nk_menu_begin_label(ctx, "Video", NK_TEXT_LEFT, nk_vec2(260, windowHeight)))
        {
            nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);
            if (nk_menu_item_label(ctx, "Scale Up", NK_TEXT_LEFT)) {
                debugger_scale_up(dbg);
            }
            if (nk_menu_item_label(ctx, "Scale Down", NK_TEXT_LEFT)) {
                debugger_scale_down(dbg);
            }
            nk_menu_end(ctx);
        }


        nk_menubar_end(ctx);
    }
    nk_end(ctx);
}
