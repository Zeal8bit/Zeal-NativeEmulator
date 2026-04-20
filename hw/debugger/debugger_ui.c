/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ui/raylib-nuklear.h"
#include "hw/zvb/zvb.h"
#include "hw/zeal.h"
#include "utils/notif.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"
#include "utils/log.h"
#include "utils/config.h"

char DEBUG_BUFFER[256];
int mouse_cursor = MOUSE_CURSOR_DEFAULT;

#ifndef ZEAL_ASSETS_DIR
#define ZEAL_ASSETS_DIR "assets"
#endif

#define BIGBLUE_DEV_PATH "assets/fonts/BigBlue_Terminal_437TT.TTF"
#define BIGBLUE_INSTALL_PATH ZEAL_ASSETS_DIR "/fonts/BigBlue_Terminal_437TT.TTF"

/**
 * ===========================================================
 *                  PANEL CONFIG
 * ===========================================================
 */

#define PANEL_DEFAULT_FLAGS ( NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE )

static struct dbg_ui_panel_t dbg_panels[] = {
    [DBG_UI_PANEL_VIDEO] = {
        .key = "P_VIDEO",
        .title = "Video",
        .render = ui_panel_display,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_NO_SCROLLBAR  ),
        .rect_default = {
            .y = MENUBAR_HEIGHT,
        },
    },
    [DBG_UI_PANEL_BKPOINT] = {
        .key = "P_BREAKPOINTS",
        .title = "Breakpoints",
        .render = ui_panel_breakpoints,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE ),
        .rect_default = { 640, MENUBAR_HEIGHT + CPU_CTRL_HEIGHT, CPU_CTRL_WIDTH, CPU_CTRL_HEIGHT - 30 },
    },
    [DBG_UI_PANEL_CPU] = {
        .key = "P_CPU",
        .title = "CPU",
        .render = ui_panel_cpu,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_TITLE ),
        .rect_default = { 640, MENUBAR_HEIGHT + 0, CPU_CTRL_WIDTH, CPU_CTRL_HEIGHT },
    },
    [DBG_UI_PANEL_MEMORY] = {
        .key = "P_MEMORY",
        .title = "Memory Viewer",
        .render = ui_panel_memory,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE ),
        .rect_default = { 0, MENUBAR_HEIGHT + 530, 640, 360 },
    },
    [DBG_UI_PANEL_DISASSEMBLER] = {
        .key = "P_DISASSEMBLER",
        .title = "Disassembler",
        .render = ui_panel_disassembler,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE ),
        .rect_default = { 640 + CPU_CTRL_WIDTH, MENUBAR_HEIGHT + 0, 390, 530 },
    },
    [DBG_UI_PANEL_VRAM] = {
        .key = "P_VRAM",
        .title = "VRAM viewer",
        .render = ui_panel_vram,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR ),
        .rect_default = { 640, MENUBAR_HEIGHT + CPU_CTRL_HEIGHT, 640, 480 },
        .hidden_default = true,
    },
    [DBG_UI_PANEL_MMU] = {
        .key = "P_MMU",
        .title = "MMU Viewer",
        .render = ui_panel_mmu,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE ),
        .rect_default = { 640, MENUBAR_HEIGHT + 530, CPU_CTRL_WIDTH, 360 },
    },
    [DBG_UI_PANEL_SEMIHOST] = {
        .key = "P_SEMIHOST",
        .title = "Semihost",
        .render = ui_panel_semihost,
        .flags = ( PANEL_DEFAULT_FLAGS | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR ),
        .rect_default = { 640 + CPU_CTRL_WIDTH, MENUBAR_HEIGHT + 530, 390, 360 },
    },
};
static const size_t dbg_panels_size = DBG_UI_PANEL_TOTAL;
static dbg_ui_panel_t *PANEL_VIDEO = &dbg_panels[DBG_UI_PANEL_VIDEO];

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

    /* Make selectable items' background transparent */
    ctx->style.selectable.normal.data.color = nk_rgba(0, 0, 0, 0);
    ctx->style.window.header.active.data.color = surface1;

}


/**
 * ===========================================================
 *                  CP437
 * ===========================================================
 */
const uint16_t s_cp437_to_unicode[256] = {
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x2302,
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
    0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
    0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
    0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
    0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};

/**
 * ===========================================================
 *                  HELPERS
 * ===========================================================
 */

bool dbg_ui_clickable_label(struct nk_context* ctx, const char* label, const char* value, bool active)
{
    nk_label(ctx, label, NK_TEXT_LEFT);
    return dbg_ui_clickable_value(ctx, value, active);
}

bool dbg_ui_clickable_value(struct nk_context* ctx, const char* value, bool active)
{
    nk_style_push_color(ctx, &ctx->style.selectable.text_hover, ctx->style.selectable.text_normal);
    nk_style_push_color(ctx, &ctx->style.selectable.text_normal, nk_rgba(0, 112, 255, 255));

    if(active) {
        dbg_ui_mouse_hover(ctx, MOUSE_POINTER);
    }
    bool ret = nk_select_label(ctx, value, NK_TEXT_LEFT, false);

    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
    return ret;
}

void dbg_ui_mouse_hover(struct nk_context *ctx, int cursor)
{
    if(nk_widget_is_hovered(ctx)) {
        mouse_cursor = cursor;
    }
}


static const char lut_hex_lower[] = "0123456789abcdef";
static const char lut_hex_upper[] = "0123456789ABCDEF";
void dbg_ui_byte_to_hex(uint8_t byte, char* out, char separator) {
    const char *lut = config.debugger.hex_upper ? lut_hex_upper : lut_hex_lower;
    out[0] = lut[byte >> 4];
    out[1] = lut[byte & 0x0F];
    if (separator != -1) {
        out[2] = separator;
    }
}

void dbg_ui_word_to_hex(uint16_t word, char* out, char separator) {
    const char *lut = config.debugger.hex_upper ? lut_hex_upper : lut_hex_lower;
    out[0] = lut[word >> 12];
    out[1] = lut[word >> 8 & 0x0F];
    out[2] = lut[word >> 4 & 0x0F];
    out[3] = lut[word & 0x0F];
    if (separator != -1) {
        out[4] = separator;
    }
}


void dbg_ui_go_to_mem(struct dbg_ui_t* dctx, hwaddr addr)
{
    dctx->mem_view_addr = addr;
}

void dbg_ui_cp437_to_utf8(uint8_t byte, char out[5])
{
    const uint16_t codepoint = s_cp437_to_unicode[byte];

    if (codepoint == 0) {
        out[0] = '.';
        out[1] = 0;
    } else if (codepoint <= 0x7F) {
        out[0] = (char)codepoint;
        out[1] = 0;
    } else if (codepoint <= 0x7FF) {
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        out[2] = 0;
    } else {
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        out[3] = 0;
    }
}

void dbg_ui_get_panel_config(dbg_ui_panel_t *panel)
{
    char key[80];

    sprintf(key, "%s_HIDDEN", panel->key);
    panel->hidden = config_get(key, panel->hidden_default);

    sprintf(key, "%s_MINIMIZED", panel->key);
    bool minimized = config_get(key, 0);
    if(minimized) {
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
        log_printf("=== Panel: %s ===\n", panel->title);
        log_printf("    width: %d\n", (int)panel->rect.w);
        log_printf("   height: %d\n", (int)panel->rect.h);
        log_printf("        x: %d\n", (int)panel->rect.x);
        log_printf("        y: %d\n", (int)panel->rect.y);
        log_printf("   hidden: %d\n", panel->hidden);
        log_printf("minimized: %d\n", !!(panel->flags & NK_WINDOW_MINIMIZED));
    }
}

int dbg_ui_config_save(rini_config *ini) {
    char key[80];

    for(size_t i = 0; i < dbg_panels_size; i++) {
        dbg_ui_panel_t *panel = &dbg_panels[i];
        rini_set_config_comment_line(ini, panel->title);

        if(panel->rect.w < 1) dbg_ui_get_panel_config(panel);

        sprintf(key, "%s_HIDDEN", panel->key);
        rini_set_config_value(ini, key, panel->hidden, "Enabled");

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

static void debugger_scale(float step)
{
    const float scale = zeal_scale_quantize_tenths(PANEL_VIDEO->rect.w / ZVB_MAX_RES_WIDTH, step);
    PANEL_VIDEO->rect.w = ZVB_MAX_RES_WIDTH * scale;
    PANEL_VIDEO->rect.h = ZVB_MAX_RES_HEIGHT * scale + NK_WIDGET_TITLE_HEIGHT;
    notif_show("Scale: x%.1f", scale);
}

void debugger_scale_up(dbg_t *dbg) {
    (void)dbg; // unreferenced
    debugger_scale(1.0f);

}
void debugger_scale_down(dbg_t *dbg) {
    (void)dbg; // unreferenced
    debugger_scale(-1.0f);
}

/**
 * ===========================================================
 *                  PUBLIC INTERFACE
 * ===========================================================
 */

int debugger_ui_init(struct dbg_ui_t** ret_ctx, const dbg_ui_init_args_t* args)
{
    if (ret_ctx == NULL || args == NULL ||
        args->main_view == NULL || args->zvb == NULL)
    {
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

    /* Load BigBlue font for the memory viewer; remains NULL if not found */
    const char *font_path = TextFormat("%s" BIGBLUE_DEV_PATH, GetApplicationDirectory());
    if (!FileExists(font_path)) {
        font_path = BIGBLUE_INSTALL_PATH;
    }
    if (FileExists(font_path)) {
        int codepoints[256];
        for (int i = 0; i < 256; i++) {
            codepoints[i] = s_cp437_to_unicode[i] ? s_cp437_to_unicode[i] : '.';
        }
        /* BigBlue Terminal block/box chars are intentionally taller than the em
         * size to tile seamlessly; suppress the harmless Raylib size warning. */
        SetTraceLogLevel(LOG_ERROR);
        Font font_bigblue = LoadFontEx(font_path, BIGBLUE_FONT_SIZE, codepoints, 256);
        SetTraceLogLevel(LOG_WARNING);
        if (font_bigblue.texture.id != 0) {
            struct Font* font_copy = (struct Font*)MemAlloc(sizeof(struct Font));
            *font_copy = font_bigblue;
            struct nk_user_font* nk_bigblue = (struct nk_user_font*)MemAlloc(sizeof(struct nk_user_font));
            nk_bigblue->userdata = nk_handle_ptr(font_copy);
            nk_bigblue->height = BIGBLUE_FONT_SIZE;
            nk_bigblue->width = nk_raylib_font_get_text_width_user_font;
            dbg_ctx->font_bigblue = nk_bigblue;
            dbg_ctx->font = font_bigblue;
        }
    } else {
        log_printf("[DEBUGGER] Missing font paths: %s, %s", BIGBLUE_DEV_PATH, BIGBLUE_INSTALL_PATH);
    }
    SetNuklearScaling(ctx, 1);

    set_theme(ctx);

    /* Convert the RayLib texture into a Nuklear image */
    dbg_ctx->view = TextureToNuklear(args->main_view->texture);

    for (int i = 0; i < args->debug_views_count && i < DBG_MAX_VRAM_VIEWS; i++) {
        dbg_ctx->vram[i] = TextureToNuklear(args->debug_views[i].texture);
    }
    dbg_ctx->zvb = args->zvb;

    /* Set the default attributes */
    dbg_ctx->mem_view_size = 256;
    dbg_ctx->mem_view_addr = 0;
    dbg_ctx->dis_addr = 0;
    dbg_ctx->dis_size = 50;

    *ret_ctx = dbg_ctx;

    // Initialize the Video panel defaults
    dbg_panels[DBG_UI_PANEL_VIDEO].rect_default.w = dbg_ctx->view.w;
    dbg_panels[DBG_UI_PANEL_VIDEO].rect_default.h = dbg_ctx->view.h + NK_WIDGET_TITLE_HEIGHT;

    for(size_t i = 0; i < dbg_panels_size; i++) {
        dbg_ui_panel_t *panel = &dbg_panels[i];
        panel->rect = panel->rect_default;
        dbg_ui_get_panel_config(panel);
    }

    return 1;
}


void debugger_ui_deinit(struct dbg_ui_t* dctx)
{
    UnloadNuklearImage(dctx->view);
    for (int i = 0; i < DBG_VIEW_TOTAL; i++) {
        UnloadNuklearImage(dctx->vram[i]);
    }
    UnloadNuklear(dctx->ctx);
    if (dctx->font.texture.id != 0) {
        UnloadFont(dctx->font);
    }
    if (dctx->font_bigblue != NULL) {
        MemFree(dctx->font_bigblue->userdata.ptr);
        MemFree(dctx->font_bigblue);
    }
    MemFree(dctx);
}

void debugger_ui_prepare_render(struct dbg_ui_t* dctx, dbg_t* dbg)
{
    UpdateNuklear(dctx->ctx);

    mouse_cursor = MOUSE_DEFAULT;

    ui_menubar(dctx, dbg, dbg_panels, dbg_panels_size);

    for(size_t i = 0; i < dbg_panels_size; i++) {
        struct nk_window *win;
        struct dbg_ui_panel_t *panel = &dbg_panels[i];

        if(panel->render == NULL || panel->hidden) {
            continue;
        }

        /* Let the windows modify their border color */
        struct nk_style_item original_active = dctx->ctx->style.window.header.active;
        struct nk_style_item original_normal = dctx->ctx->style.window.header.normal;
        if (panel->override_header_style) {
            dctx->ctx->style.window.header.active = panel->header_style;
            dctx->ctx->style.window.header.normal = panel->header_style;
        }
        if(nk_begin(dctx->ctx, panel->title, panel->rect, panel->flags)) {
            panel->render(panel, dctx, dbg);
            struct nk_rect bounds = nk_window_get_bounds(dctx->ctx);
            panel->rect.x = bounds.x;
            panel->rect.y = bounds.y;
            if(panel->flags & NK_WINDOW_SCALABLE) {
                panel->rect.w = bounds.w;
                panel->rect.h = bounds.h;
            }
        }
        win = dctx->ctx->current;
        nk_end(dctx->ctx);
        /* Restore the original border color */
        dctx->ctx->style.window.header.active = original_active;
        dctx->ctx->style.window.header.normal = original_normal;

        if(win->flags & NK_WINDOW_MINIMIZED) {
            panel->flags |= NK_WINDOW_MINIMIZED;
        } else {
            panel->flags &= ~(NK_WINDOW_MINIMIZED);
        }
        panel->hidden = !!(win->flags & NK_WINDOW_HIDDEN);

        if(win->bounds.y < MENUBAR_HEIGHT) {
            win->bounds.y = MENUBAR_HEIGHT;
        }

        if(!(panel->flags & NK_WINDOW_SCALABLE)) {
            win->bounds = panel->rect;
        }
    }


    SetMouseCursor(mouse_cursor);
}


void debugger_ui_render(struct dbg_ui_t* dctx, dbg_t* dbg)
{
    (void) dbg;
    DrawNuklear(dctx->ctx);
}


bool debugger_ui_main_view_focused(const struct dbg_ui_t* dctx)
{
    return nk_window_is_active(dctx->ctx, PANEL_VIDEO->title);
}

bool debugger_ui_vram_panel_opened(const struct dbg_ui_t* dctx)
{
    (void) dctx;
    return !dbg_panels[DBG_UI_PANEL_VRAM].hidden;
}
