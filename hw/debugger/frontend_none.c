/* SPDX-License-Identifier: Apache-2.0 */
#include "debugger/frontend.h"
int debugger_ui_init(dbg_ui_t **ui, const dbg_ui_init_args_t *a)
{
    (void)a;
    *ui = 0;
    return 0;
}
void debugger_ui_deinit(dbg_ui_t *u) { (void)u; }
void debugger_ui_show(dbg_ui_t *u, bool b)
{
    (void)u;
    (void)b;
}
void debugger_ui_poll(dbg_ui_t *u) { (void)u; }
void debugger_ui_refresh(dbg_ui_t *u) { (void)u; }
bool debugger_ui_main_view_focused(const dbg_ui_t *u)
{
    (void)u;
    return true;
}
dbg_vram_t debugger_ui_vram_panel_opened(const dbg_ui_t *u)
{
    (void)u;
    return DBG_VIEW_NONE;
}
void debugger_ui_scale(dbg_ui_t *u, int d)
{
    (void)u;
    (void)d;
}
