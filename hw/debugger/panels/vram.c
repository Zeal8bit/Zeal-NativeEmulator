/**
 * ===========================================================
 *                  ZEAL VIDEO BOARD
 * ===========================================================
 */

#include <string.h>
#include "ui/nuklear.h"
#include "ui/raylib-nuklear.h"
#include "debugger/debugger_types.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"

#ifndef MAX
    #define MAX(a,b)    ((a) > (b) ? (a) : (b))
#endif

typedef enum {
    TAB_LAYER0,
    TAB_LAYER1,
    TAB_TILESET,
    TAB_PALETTE,
    TAB_FONT,
    TAB_COUNT,
} tab_t;

static const char *s_tab_names[] = {
    [TAB_LAYER0]  = "Layer0",
    [TAB_LAYER1]  = "Layer1",
    [TAB_TILESET] = "Tileset",
    [TAB_PALETTE] = "Palette",
    [TAB_FONT]    = "Font",
};
static const struct nk_color s_active_color   = { .r = 0, .g = 0x2d, .b = 0x78, .a = 0xff };
static const struct nk_color s_inactive_color = { .r = 0x6c, .g = 0x70, .b = 0x86, .a = 0xff };

#define TILE_WIDTH  16
#define TILE_HEIGHT 16
#define TILES_X     80
#define TILES_Y     40
#define GRID_COLOR  nk_rgb(180, 180, 180)
#define BOLD_COLOR  nk_rgb(100, 100, 100)
// Total size
#define VIEW_WIDTH  (TILE_WIDTH * TILES_X)   // 1280
#define VIEW_HEIGHT (TILE_HEIGHT * TILES_Y)  // 640

static void get_tile_size(const zvb_t* zvb, int* width, int* height)
{
    if (zvb_is_text_mode(zvb)) {
        *width = TEXT_CHAR_WIDTH;
        *height = TEXT_CHAR_HEIGHT;
    } else {
        *width = TILE_WIDTH;
        *height = TILE_HEIGHT;
    }
}

static void ui_tab_layer(struct dbg_ui_t* dctx, int layer)
{
    struct nk_context* ctx = dctx->ctx;
    struct nk_image* img = &dctx->vram[layer > 0 ? DBG_TILEMAP_LAYER1 : DBG_TILEMAP_LAYER0];

    nk_layout_row_static(ctx, img->h, img->w, 1);
    struct nk_rect image_bounds = nk_widget_bounds(ctx);
    nk_image(ctx, *img);
    const struct nk_mouse* mouse = &ctx->input.mouse;

    if (nk_input_is_mouse_hovering_rect(&ctx->input, image_bounds)) {
        float rel_x = mouse->pos.x - image_bounds.x;
        float rel_y = mouse->pos.y - image_bounds.y;

        int width_w_grid = 0;
        int height_w_grid = 0;
        get_tile_size(dctx->zvb, &width_w_grid, &height_w_grid);
        width_w_grid += 1;
        height_w_grid += 1;

        int tile_x = (int)(rel_x / width_w_grid);
        int tile_y = (int)(rel_y / height_w_grid);

        // Get the subimage of the hovered tile
        struct nk_image tile_img = nk_subimage_ptr(
            img->handle.ptr,
            img->w, img->h,
            (struct nk_rect){
                tile_x * width_w_grid,
                tile_y * height_w_grid,
                width_w_grid + 1, height_w_grid + 1
            }
        );

        /* Tooltip dimensions */
        const float x_padding = 14.0f;
        const float y_padding = 4.0f;
        const float text_height = 20.0f;
        const float text_width = 100.0f;
        const float tile_w = width_w_grid * 4;
        const float tile_h = height_w_grid * 4;
        const float tooltip_w = MAX(text_width, tile_w) + x_padding * 2;
        const float tooltip_h = tile_h + text_height + y_padding * 2;

        struct nk_rect tooltip_rect = nk_rect(
            mouse->pos.x + 8,
            mouse->pos.y + 8,
            tooltip_w,
            tooltip_h
        );

        struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);

        /* Draw tooltip background */
        nk_fill_rect(canvas, tooltip_rect, 2.0f, nk_rgba(40, 40, 40, 230));
        /* Draw the text first (at the top) */
        char buf[64];
        snprintf(buf, sizeof(buf), "(%d,%d) [%d]",
                    tile_x, tile_y, tile_x + TILES_X * tile_y);
        struct nk_rect text_rect = nk_rect(tooltip_rect.x + x_padding, tooltip_rect.y + y_padding,
                                           text_width, text_height);
        nk_draw_text(canvas, text_rect, buf, strlen(buf),
                     ctx->style.font, nk_rgba(0,0,0,0), nk_rgba(255,255,255,255));
        /* Draw tile image */
        struct nk_rect tile_rect = nk_rect(tooltip_rect.x + tooltip_w / 2 - tile_w / 2,
                                           text_rect.y + text_height + 0.5f,
                                           tile_w, tile_h);
        nk_draw_image(canvas, tile_rect, &tile_img, (struct nk_color) {255,255,255,255});
    }
}


/**
 * @brief Display any set, such as tileset or palette
 */
static void ui_tab_set(struct dbg_ui_t* dctx, struct nk_image* img, const char* entry_name)
{
    struct nk_context* ctx = dctx->ctx;

    const float set_scale = 2.0f;
    const int tile_scale = 10;
    const int tile_per_line = 16;

    nk_layout_row_static(ctx, img->h * set_scale, img->w * set_scale, 2);
    struct nk_rect image_bounds = nk_widget_bounds(ctx);
    nk_image(ctx, *img);

    struct nk_mouse* mouse = &ctx->input.mouse;

    if (nk_input_is_mouse_hovering_rect(&ctx->input, image_bounds)) {
        float rel_x = (mouse->pos.x - image_bounds.x) / set_scale;
        float rel_y = (mouse->pos.y - image_bounds.y) / set_scale;

        int width_w_grid = 0;
        int height_w_grid = 0;
        get_tile_size(dctx->zvb, &width_w_grid, &height_w_grid);
        width_w_grid += 1;
        height_w_grid += 1;

        int tile_x = (int)(rel_x / width_w_grid);
        int tile_y = (int)(rel_y / height_w_grid);

        struct nk_image sub = nk_subimage_ptr(
            img->handle.ptr,
            width_w_grid + 1, height_w_grid + 1,
            (struct nk_rect){
                tile_x * width_w_grid,
                tile_y * height_w_grid,
                /* Also show the right border of the tile */
                width_w_grid + 1,
                height_w_grid + 1
            }
        );

        if (nk_group_begin(ctx, "right_group", NK_WINDOW_NO_SCROLLBAR)) {
            char label[64];
            nk_layout_row_dynamic(ctx, 30, 1);
            snprintf(label, sizeof(label), "%s %d", entry_name, tile_y * tile_per_line + tile_x);
            nk_label(ctx, label, NK_TEXT_LEFT);
            nk_layout_row_static(ctx, sub.h * tile_scale, sub.w * tile_scale, 1);
            nk_image(ctx, sub);
            nk_group_end(ctx);
        }
    }
}


void ui_panel_vram(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    static int current_tab = 0;
    struct nk_context* ctx = dctx->ctx;

    nk_layout_row_dynamic(ctx, 30, TAB_COUNT);

    for (int i = 0; i < TAB_COUNT; ++i) {
        if (i == current_tab) {
            nk_style_push_color(ctx, &ctx->style.button.normal.data.color, s_active_color);
        } else {
            nk_style_push_color(ctx, &ctx->style.button.normal.data.color, s_inactive_color);
        }

        if (nk_button_label(ctx, s_tab_names[i])) {
            current_tab = i;
        }

        nk_style_pop_color(ctx);
    }

    // Content Area
    nk_layout_row_dynamic(ctx, panel->rect.h - 85, 1);

    /* If we are in bitmap mode, we don't have access to these */
    if (zvb_is_bitmap_mode(dctx->zvb)) {
        return;
    }

    if (nk_group_begin_titled(ctx, "tab_content", s_tab_names[current_tab], 0)) {
        switch (current_tab) {
            case TAB_LAYER0:
                ui_tab_layer(dctx, 0);
                break;
            case TAB_LAYER1:
                if (zvb_is_gfx_mode(dctx->zvb)) {
                    ui_tab_layer(dctx, current_tab == TAB_LAYER1);
                }
                break;
            case TAB_TILESET:
                if (zvb_is_gfx_mode(dctx->zvb)) {
                    ui_tab_set(dctx, &dctx->vram[DBG_TILESET], "Tile Index");
                }
                break;
            case TAB_PALETTE:
                /* TODO: implement palette for text mode too? */
                if (zvb_is_gfx_mode(dctx->zvb)) {
                    ui_tab_set(dctx, &dctx->vram[DBG_PALETTE], "Color");
                }
                break;
            case TAB_FONT:
                ui_tab_set(dctx, &dctx->vram[DBG_FONT], "Char");
                break;
        }
        nk_group_end(ctx);
    }

    (void) panel;
    (void) dbg;
}
