/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * ===========================================================
 *                  MEMORY VIEWER
 * ===========================================================
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"

#define PANEL_FLAGS ( NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE )

#define ROWS        16   // Number of rows visible at a time
#define COLS        16   // Bytes per row
#define MEM_SIZE    256

#define BOTTOM_LINES    2
#define BOTTOM_HEIGHT   (BOTTOM_LINES) * 50

static float ui_memory_text_width(struct nk_context* ctx, const char* text, float padding)
{
    const struct nk_user_font* font = ctx->style.font;

    if (font == NULL || font->width == NULL) {
        return (float)strlen(text) * 8.0f + padding;
    }

    return font->width(font->userdata, font->height, text, (int)strlen(text)) + padding;
}

void ui_panel_memory(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    (void)panel; // unreferenced
    struct nk_context* ctx = dctx->ctx;

    const float buttons_height = 85;
    struct nk_rect parent_bounds = nk_window_get_bounds(ctx);
    float scroll_height = parent_bounds.h - buttons_height;

    nk_layout_row_dynamic(ctx, scroll_height, 1);

    /* Scrollable region */
    if (nk_group_begin(ctx, "Memory_Scroll", NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 10, 1);

        const float addr_col_width = ui_memory_text_width(ctx, "0000", 6.0f);
        const float byte_col_width = ui_memory_text_width(ctx, "00", 2.0f);
        const float spacer_col_width = ui_memory_text_width(ctx, "|", 2.0f);
        const float ascii_gap_width = ui_memory_text_width(ctx, "  ", 0.0f);
        const float ascii_char_width = ui_memory_text_width(ctx, "W", 0.0f);
        const float row_height = 18.0f;
        const struct nk_color ascii_highlight_bg = ctx->style.selectable.hover.data.color;
        const struct nk_color ascii_normal_bg = nk_rgba(0, 0, 0, 0);
        const struct nk_color highlight_text = nk_rgba(255, 0, 255, 255);
        static int hovered_row_addr = -1;
        static int hovered_col = -1;

        char addr_text[16] = {0};
        char byte_text[3] = {0};
        char ascii_char[2] = {0};

        /* Retrieve the memory data */
        uint8_t mem[MEM_SIZE];
        debugger_read_memory(dbg, dctx->mem_view_addr, MEM_SIZE, mem);
        bool hover_found = false;

        for (int i = 0; i < MEM_SIZE; i += COLS) {
            const int current_addr = dctx->mem_view_addr + i;
            int next_hovered_byte = -1;

            nk_layout_row_begin(ctx, NK_STATIC, row_height, 35);
            nk_style_push_vec2(ctx, &ctx->style.selectable.padding, nk_vec2(0, 0));

            snprintf(addr_text, sizeof(addr_text), "%04X", current_addr);
            nk_layout_row_push(ctx, addr_col_width);
            if (dbg_ui_clickable_value(ctx, addr_text, true)) {
                dbg_ui_go_to_mem(dctx, current_addr);
            }

            for (int j = 0; j < COLS; j++) {
                _Static_assert(MEM_SIZE % COLS == 0, "The memory dump size must be a multiple of columns");
                const uint8_t byte = mem[i + j];
                if (j == 8) {
                    nk_layout_row_push(ctx, spacer_col_width);
                    nk_label(ctx, "|", NK_TEXT_CENTERED);
                }
                dbg_ui_byte_to_hex(byte, byte_text, -1);
                nk_layout_row_push(ctx, byte_col_width);
                const struct nk_color bg =
                    (hovered_row_addr == current_addr && hovered_col == j) ? ascii_highlight_bg : ascii_normal_bg;
                nk_style_push_style_item(ctx, &ctx->style.selectable.normal,
                                         nk_style_item_color(bg));
                nk_style_push_style_item(ctx, &ctx->style.selectable.hover,
                                         nk_style_item_color(bg));
                nk_style_push_style_item(ctx, &ctx->style.selectable.pressed,
                                         nk_style_item_color(bg));
                if (hovered_row_addr == current_addr && hovered_col == j) {
                    nk_style_push_color(ctx, &ctx->style.selectable.text_normal, highlight_text);
                    nk_style_push_color(ctx, &ctx->style.selectable.text_hover, highlight_text);
                    nk_style_push_color(ctx, &ctx->style.selectable.text_pressed, highlight_text);
                }
                struct nk_rect byte_bounds = nk_widget_bounds(ctx);
                if (nk_input_is_mouse_hovering_rect(&ctx->input, byte_bounds)) {
                    next_hovered_byte = j;
                }
                nk_select_label(ctx, byte_text, NK_TEXT_CENTERED, false);
                if (hovered_row_addr == current_addr && hovered_col == j) {
                    nk_style_pop_color(ctx);
                    nk_style_pop_color(ctx);
                    nk_style_pop_color(ctx);
                }
                nk_style_pop_style_item(ctx);
                nk_style_pop_style_item(ctx);
                nk_style_pop_style_item(ctx);
                /* Check if the character is printable */
                ascii_char[0] = isprint((char) byte) ? byte : '.';
                ascii_char[1] = 0;
            }
            nk_layout_row_push(ctx, ascii_gap_width);
            nk_label(ctx, "", NK_TEXT_LEFT);
            for (int j = 0; j < COLS; j++) {
                const uint8_t byte = mem[i + j];
                ascii_char[0] = isprint((char) byte) ? byte : '.';
                ascii_char[1] = 0;
                nk_layout_row_push(ctx, ascii_char_width);
                const struct nk_color bg =
                    (hovered_row_addr == current_addr && hovered_col == j) ? ascii_highlight_bg : ascii_normal_bg;
                nk_style_push_style_item(ctx, &ctx->style.selectable.normal,
                                         nk_style_item_color(bg));
                nk_style_push_style_item(ctx, &ctx->style.selectable.hover,
                                         nk_style_item_color(bg));
                nk_style_push_style_item(ctx, &ctx->style.selectable.pressed,
                                         nk_style_item_color(bg));
                if (hovered_row_addr == current_addr && hovered_col == j) {
                    nk_style_push_color(ctx, &ctx->style.selectable.text_normal, highlight_text);
                    nk_style_push_color(ctx, &ctx->style.selectable.text_hover, highlight_text);
                    nk_style_push_color(ctx, &ctx->style.selectable.text_pressed, highlight_text);
                }
                struct nk_rect ascii_bounds = nk_widget_bounds(ctx);
                if (nk_input_is_mouse_hovering_rect(&ctx->input, ascii_bounds)) {
                    next_hovered_byte = j;
                }
                nk_select_label(ctx, ascii_char, NK_TEXT_CENTERED, false);
                if (hovered_row_addr == current_addr && hovered_col == j) {
                    nk_style_pop_color(ctx);
                    nk_style_pop_color(ctx);
                    nk_style_pop_color(ctx);
                }
                nk_style_pop_style_item(ctx);
                nk_style_pop_style_item(ctx);
                nk_style_pop_style_item(ctx);
            }
            nk_layout_row_end(ctx);
            nk_style_pop_vec2(ctx);

            if (next_hovered_byte >= 0) {
                hovered_row_addr = current_addr;
                hovered_col = next_hovered_byte;
                hover_found = true;
            }
        }

        if (!hover_found) {
            hovered_row_addr = -1;
            hovered_col = -1;
        }

        nk_group_end(ctx);
    }

    /* Add a line for the address to check and the dump */
    nk_layout_row_dynamic(ctx, 30, 2);
    static char edit_addr[64] = { 0 };
    dbg_ui_mouse_hover(ctx, MOUSE_TEXT);
    nk_flags flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, edit_addr, sizeof(edit_addr), NULL);

    /**
     * Show a 'View' button to dump the memory
     * Parse the typed address if Enter was pressed or the buttonw as clicked
     */
    dbg_ui_mouse_hover(ctx, MOUSE_POINTER);
    if ((nk_button_label(ctx, "View") || (flags & NK_EDIT_COMMITED))) {
        /* Make sure it is a hex value */
        hwaddr addr = 0;
        if (sscanf(edit_addr, "%x", &addr) == 1 || debugger_find_symbol(dbg, edit_addr, &addr)) {
            dctx->mem_view_addr = (int) addr;
        }
    }
}
