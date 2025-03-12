/**
 * ===========================================================
 *                  BREAKPOINTS
 * ===========================================================
 */

#include <stdio.h>
#include <string.h>
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"

#define PANEL_FLAGS ( NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE )

void ui_panel_breakpoints(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    (void)panel; // unreferenced
    struct nk_context* ctx = dctx->ctx;
    /* Input buffer used for adding a new breakpoint, it must be kept alive
     * through all the calls, so put it in static. */
    static char input[32] = { 0 };
    hwaddr new_bp;

    /* Get the breakpoints from the debugger core */
    hwaddr breakpoints[DBG_MAX_POINTS];
    int bp_len = debugger_get_breakpoints(dbg, breakpoints, DBG_MAX_POINTS);

    /* Height of the field at the bottom that lets the user add a breakpoint */
    const float add_field_height = 85;
    struct nk_rect parent_bounds = nk_window_get_bounds(ctx); // Get window size
    float scroll_height = parent_bounds.h - add_field_height;
    nk_layout_row_dynamic(ctx, scroll_height, 1); // This ensures the scroll area takes space

    /* Create a new group to have a scroll section */
    if (nk_group_begin(ctx, "BreakpointsList", NK_WINDOW_BORDER)) {

        for (int i = 0; i < bp_len; i++) {

            /* Let's make a custom row to have a smaller 'x' button */
            nk_layout_row_begin(ctx, NK_STATIC, 25, 2);
            nk_layout_row_push(ctx, parent_bounds.w - 80);

            snprintf(DEBUG_BUFFER, sizeof(DEBUG_BUFFER), "0x%04X", breakpoints[i]);
            nk_label(ctx, DEBUG_BUFFER, NK_TEXT_LEFT);


            nk_layout_row_push(ctx, 30);
            if (nk_button_label(ctx, "x")) {
                debugger_clear_breakpoint(dbg, breakpoints[i]);
            }
            nk_layout_row_end(ctx);
        }

        nk_group_end(ctx);
    }


    nk_layout_row_dynamic(ctx, 30, 2);
    /* Fixed input row at the bottom */
    dbg_ui_mouse_hover(ctx, MOUSE_TEXT);
    nk_flags flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, input, 32, NULL);

    /* Show an 'Add' button to set a breakpoint */
    dbg_ui_mouse_hover(ctx, MOUSE_POINTER);
    if (nk_button_label(ctx, "Add") || (flags & NK_EDIT_COMMITED)) {
        /* Make sure it is a hex value (for now) */
        if (sscanf(input, "%x", &new_bp) == 1 || debugger_find_symbol(dbg, input, &new_bp)) {
            /* Clear the buffer to clear the field */
            memset(input, 0, sizeof(input));
            debugger_set_breakpoint(dbg, new_bp);
        }
    }
}
