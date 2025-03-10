#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"
#include "utils/config.h"

char DEBUG_BUFFER[256];

/**
 * ===========================================================
 *                  PANEL CONFIG
 * ===========================================================
 */

#define PANEL_DEFAULT_FLAGS ( NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE )

struct dbg_ui_panel_t dbg_panels[] = {
    [0] = {
        .key = "P_VIDEO",
        .title = "Video",
        .render = ui_panel_display,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_NO_SCROLLBAR  ),
        .rect_default = {
            .y = MENUBAR_HEIGHT,
        },
    },
    {
        .key = "P_BREAKPOINTS",
        .title = "Breakpoints",
        .render = ui_panel_breakpoints,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE ),
        .rect_default = { 640, MENUBAR_HEIGHT + CPU_CTRL_HEIGHT, CPU_CTRL_WIDTH, CPU_CTRL_HEIGHT - 30 },
    },
    {
        .key = "P_CPU",
        .title = "CPU",
        .render = ui_panel_cpu,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_TITLE ),
        .rect_default = { 640, MENUBAR_HEIGHT + 0, CPU_CTRL_WIDTH, CPU_CTRL_HEIGHT },
    },
    {
        .key = "P_MEMORY",
        .title = "Memory Viewer",
        .render = ui_panel_memory,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE ),
        .rect_default = { 0, MENUBAR_HEIGHT + 530, 640 + CPU_CTRL_WIDTH, 300 },
    },
    {
        .key = "P_DISASSEMBLER",
        .title = "Disassembler",
        .render = ui_panel_disassembler,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_TITLE ),
        .rect_default = { 640 + CPU_CTRL_WIDTH, MENUBAR_HEIGHT + 0, 390, 830 },
    },
};
size_t dbg_panels_size = sizeof(dbg_panels) / sizeof(struct dbg_ui_panel_t);
dbg_ui_panel_t *PANEL_VIDEO = &dbg_panels[0]; // the panel containing ZVB output

/**
 * ===========================================================
 *                  STYLING
 * ===========================================================
 */

void set_theme(struct nk_context *ctx) {

    // set the default style first
    nk_style_default(ctx);

    struct nk_color table[NK_COLOR_COUNT];

    struct nk_color pink = nk_rgba(245, 194, 231, 255);
    struct nk_color green = nk_rgba(166, 227, 161, 255);
    struct nk_color lavender = nk_rgba(180, 190, 254, 255);
    struct nk_color text     = nk_rgba(205, 214, 244, 255);
    struct nk_color overlay2 = nk_rgba(147, 153, 178, 255);
    struct nk_color overlay1 = nk_rgba(127, 132, 156, 255);
    struct nk_color overlay0 = nk_rgba(108, 112, 134, 255);
    struct nk_color surface2 = nk_rgba(88, 91, 112, 255);
    struct nk_color surface1 = nk_rgba(69, 71, 90, 255);
    struct nk_color surface0 = nk_rgba(49, 50, 68, 255);
    struct nk_color base     = nk_rgba(30, 30, 46, 255);
    struct nk_color mantle   = nk_rgba(24, 24, 37, 255);

    /*struct nk_color crust = nk_rgba(17, 17, 27, 255);*/
    table[NK_COLOR_TEXT]                    = text;
    table[NK_COLOR_WINDOW]                  = base;
    table[NK_COLOR_HEADER]                  = mantle;
    table[NK_COLOR_BORDER]                  = mantle;
    table[NK_COLOR_BUTTON]                  = surface0;
    table[NK_COLOR_BUTTON_HOVER]            = overlay1;
    table[NK_COLOR_BUTTON_ACTIVE]           = overlay0;
    table[NK_COLOR_TOGGLE]                  = surface2;
    table[NK_COLOR_TOGGLE_HOVER]            = overlay2;
    table[NK_COLOR_TOGGLE_CURSOR]           = lavender;
    table[NK_COLOR_SELECT]                  = surface0;
    table[NK_COLOR_SELECT_ACTIVE]           = overlay0;
    table[NK_COLOR_SLIDER]                  = surface1;
    table[NK_COLOR_SLIDER_CURSOR]           = green;
    table[NK_COLOR_SLIDER_CURSOR_HOVER]     = green;
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = green;
    table[NK_COLOR_PROPERTY]                = surface0;
    table[NK_COLOR_EDIT]                    = surface0;
    table[NK_COLOR_EDIT_CURSOR]             = lavender;
    table[NK_COLOR_COMBO]                   = surface0;
    table[NK_COLOR_CHART]                   = surface0;
    table[NK_COLOR_CHART_COLOR]             = lavender;
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = pink;
    table[NK_COLOR_SCROLLBAR]               = surface0;
    table[NK_COLOR_SCROLLBAR_CURSOR]        = overlay0;
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = lavender;
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = pink;
    table[NK_COLOR_TAB_HEADER]              = surface0;
    table[NK_COLOR_KNOB]                    = table[NK_COLOR_SLIDER];
    table[NK_COLOR_KNOB_CURSOR]             = pink;
    table[NK_COLOR_KNOB_CURSOR_HOVER]       = pink;
    table[NK_COLOR_KNOB_CURSOR_ACTIVE]      = pink;
    nk_style_from_table(ctx, table);
}

/**
 * ===========================================================
 *                  HELPERS
 * ===========================================================
 */

bool dbg_ui_clickable_label(struct nk_context* ctx, const char* label, const char* value)
{
    nk_label(ctx, label, NK_TEXT_LEFT);
    nk_style_push_color(ctx, &ctx->style.selectable.text_hover, nk_rgba(120, 120, 120, 255));
    bool ret = nk_select_label(ctx, value, NK_TEXT_LEFT, false);
    nk_style_pop_color(ctx);
    return ret;
}


void dbg_ui_byte_to_hex(uint8_t byte, char* out, char separator) {
    static const char lut[] = "0123456789abcdef";
    out[0] = lut[byte >> 4];
    out[1] = lut[byte & 0x0F];
    if (separator != -1) {
        out[2] = separator;
    }
}


void dbg_ui_go_to_mem(struct dbg_ui_t* dctx, hwaddr addr)
{
    dctx->mem_view_addr = addr;
}

void dbg_ui_get_panel_config(dbg_ui_panel_t *panel)
{
    char key[80];

    sprintf(key, "%s_HIDDEN", panel->key);
    if(config_get(key, 0)) {
        panel->flags |= NK_WINDOW_HIDDEN;
    }

    sprintf(key, "%s_MINIMIZED", panel->key);
    if(config_get(key, 0)) {
        panel->flags |= NK_WINDOW_MINIMIZED;
    }

    sprintf(key, "%s_POS_X", panel->key);
    panel->rect.x = config_get(key, panel->rect.x);
    sprintf(key, "%s_POS_Y", panel->key);
    panel->rect.y = config_get(key, panel->rect.y);
    sprintf(key, "%s_WIDTH", panel->key);
    panel->rect.w = config_get(key, panel->rect.w);
    sprintf(key, "%s_HEIGHT", panel->key);
    panel->rect.h = config_get(key, panel->rect.h);

    if(config.arguments.verbose) {
        printf("=== Panel: %s ===\n", panel->title);
        printf("    width: %d\n", (int)panel->rect.w);
        printf("   height: %d\n", (int)panel->rect.h);
        printf("        x: %d\n", (int)panel->rect.x);
        printf("        y: %d\n", (int)panel->rect.y);
        printf("   hidden: %d\n", !!(panel->flags & NK_WINDOW_HIDDEN));
        printf("minimized: %d\n", !!(panel->flags & NK_WINDOW_MINIMIZED));
    }
}

int dbg_ui_config_save(rini_config *ini) {
    char key[80];

    for(size_t i = 0; i < dbg_panels_size; i++) {
        dbg_ui_panel_t *panel = &dbg_panels[i];
        rini_set_config_comment_line(ini, panel->title);

        if(panel->rect.w < 1) dbg_ui_get_panel_config(panel);

        sprintf(key, "%s_HIDDEN", panel->key);
        rini_set_config_value(ini, key, !!(panel->flags & NK_WINDOW_HIDDEN), "Enabled");

        sprintf(key, "%s_MINIMIZED", panel->key);
        rini_set_config_value(ini, key, !!(panel->flags & NK_WINDOW_MINIMIZED), "Minimized");

        sprintf(key, "%s_POS_X", panel->key);
        rini_set_config_value(ini, key, panel->rect.x, "X");
        sprintf(key, "%s_POS_Y", panel->key);
        rini_set_config_value(ini, key, panel->rect.y, "Y");
        sprintf(key, "%s_WIDTH", panel->key);
        rini_set_config_value(ini, key, panel->rect.w, "Width");
        sprintf(key, "%s_HEIGHT", panel->key);
        rini_set_config_value(ini, key, panel->rect.h, "Height");
    }
    return 0; // TODO: error checking?
}

void dbg_ui_update_cursor(struct nk_context *ctx, struct nk_rect rect) {
    struct nk_input *input = &ctx->input;
    int cursorType = MOUSE_CURSOR_DEFAULT;

    if (nk_input_is_mouse_hovering_rect(input, rect)) {
        cursorType = MOUSE_CURSOR_POINTING_HAND; // Pointer when hovering
    }

    SetMouseCursor(cursorType);
}

/**
 * ===========================================================
 *                  PUBLIC INTERFACE
 * ===========================================================
 */

int debugger_ui_init(struct dbg_ui_t** ret_ctx, RenderTexture2D* emu_view)
{
    if (ret_ctx == NULL) {
        return 0;
    }

    struct dbg_ui_t* dbg_ctx = MemAlloc(sizeof(struct dbg_ui_t));
    if (dbg_ctx == NULL) {
        return 0;
    }

    Font font = LoadFontFromNuklear(WIN_UI_FONT_SIZE);
    struct nk_context* ctx = InitNuklearEx(font, WIN_UI_FONT_SIZE);
    if (ctx == NULL) {
        MemFree(dbg_ctx);
        return 0;
    }
    dbg_ctx->ctx = ctx;
    SetNuklearScaling(ctx, 1);

    set_theme(ctx);

    /* Convert the RayLib texture into a Nuklear image */
    dbg_ctx->view = TextureToNuklear(emu_view->texture);

    /* Set the default attributes */
    dbg_ctx->mem_view_size = 256;
    dbg_ctx->mem_view_addr = 0;
    dbg_ctx->dis_addr = 0;
    dbg_ctx->dis_size = 50;

    *ret_ctx = dbg_ctx;

    // Initialize the Video panel defaults
    dbg_panels[0].rect_default.w = dbg_ctx->view.w;
    dbg_panels[0].rect_default.h = dbg_ctx->view.h + 50;

    for(size_t i = 0; i < dbg_panels_size; i++) {
        dbg_ui_panel_t *panel = &dbg_panels[i];
        panel->rect = panel->rect_default;
        dbg_ui_get_panel_config(panel);
    }

    return 1;
}


void debugger_ui_deinit(struct dbg_ui_t* ctx)
{
    UnloadNuklearImage(ctx->view);
    UnloadNuklear(ctx->ctx);
    MemFree(ctx);
}

void debugger_ui_prepare_render(struct dbg_ui_t* ctx, dbg_t* dbg)
{
    UpdateNuklear(ctx->ctx);

    for(size_t i = 0; i < dbg_panels_size; i++) {
        struct dbg_ui_panel_t *panel = &dbg_panels[i];
        if(panel->render == NULL) continue;

        struct nk_window *win;
        if(nk_begin(ctx->ctx, panel->title, panel->rect, panel->flags)) {
            panel->render(panel, ctx, dbg);
            panel->rect = nk_window_get_bounds(ctx->ctx);
        }
        win = ctx->ctx->current;
        nk_end(ctx->ctx);

        panel->flags = win->flags;
        if(win->bounds.y < MENUBAR_HEIGHT) {
            win->bounds.y = MENUBAR_HEIGHT;
        }
    }

    ui_menubar(ctx, dbg, dbg_panels, dbg_panels_size);
}


void debugger_ui_render(struct dbg_ui_t* ctx, dbg_t* dbg)
{
    (void) dbg;
    DrawNuklear(ctx->ctx);
}


bool debugger_ui_main_view_focused(const struct dbg_ui_t* ctx)
{
    return nk_window_is_active(ctx->ctx, PANEL_VIDEO->title);
}
