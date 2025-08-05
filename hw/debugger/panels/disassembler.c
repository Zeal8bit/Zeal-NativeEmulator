/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * ===========================================================
 *                  DISASSEMBLER VIEW
 * ===========================================================
 */

#include <stdio.h>
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"

#define PANEL_FLAGS             ( NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE )
#define DISASSEMBLY_LINE_HEIGHT 15

typedef struct {
    /* Colors for breakpoints */
    struct nk_color bps_bg_color;
    struct nk_color bps_hover_color;
    /* Same for the current instruction */
    struct nk_color cur_bg_color;
    struct nk_color cur_hover_color;
    /* Color for regualr code */
    struct nk_color reg_color;
} Colors;

Colors colors = {
    .bps_bg_color = { .r = 0xa4, .g = 0, .b = 0x0f, .a = 0xff },
    .bps_hover_color = { .r = 0xbb, .g = 0, .b = 0x12, .a = 0xff },
    .cur_bg_color = { .r = 0, .g = 0x2d, .b = 0x78, .a = 0xff },
    .cur_hover_color = { .r = 0, .g = 0x45, .b = 0xb8, .a = 0xff },
    .reg_color = { .r = 0xee, .g = 0xee, .b = 0xee, .a = 0xff },
};

void ui_panel_disassembler(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    (void)panel; // unreferenced
    struct nk_context* ctx = dctx->ctx;
    const hwaddr pc = dctx->dis_addr;
    dbg_instr_t instr;

    /* Show a few instructions before PC */
    hwaddr current_addr = dctx->dis_addr; /*(dctx->dis_addr < 8) ? dctx->dis_addr : dctx->dis_addr - 8;*/

    for (size_t i = 0; i < dctx->dis_size; i++) {

        bool pushed = false;
        bool is_breakpoint = debugger_is_breakpoint_set(dbg, current_addr);

        /* Instruction Column, get teh current instruction */
        int instr_bytes = debugger_disassemble_address(dbg, current_addr, &instr);

        /* Check if there is a label */
        if (instr.label[0]) {
            nk_layout_row_dynamic(ctx, DISASSEMBLY_LINE_HEIGHT, 1);
            nk_label(ctx, instr.label, NK_TEXT_LEFT);
        }

        /* Generate an address-instruction pair line */
        nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
        nk_layout_row_push(ctx, 20);
        bool button_clicked = false;

        if (pc == current_addr) {
            pushed = true;
            button_clicked = nk_button_color(ctx, colors.cur_bg_color);
            /* Change the background color for the current instruction */
            nk_style_push_color(ctx, &ctx->style.selectable.normal.data.color, colors.cur_bg_color);
            nk_style_push_color(ctx, &ctx->style.selectable.hover.data.color, colors.cur_hover_color);
        } else if (is_breakpoint) {
            /* Change the background for the breakpoints */
            pushed = true;
            button_clicked = nk_button_color(ctx, colors.bps_bg_color);
            nk_style_push_color(ctx, &ctx->style.selectable.normal.data.color, colors.bps_bg_color);
            nk_style_push_color(ctx, &ctx->style.selectable.hover.data.color, colors.bps_hover_color);
        } else {
            button_clicked = nk_button_color(ctx, colors.reg_color);
        }

        if (button_clicked) {
            /* Button was clicked, toggle the breakpoint! */
            debugger_toggle_breakpoint(dbg, current_addr);
        }

        nk_layout_row_push(ctx, 370);

        snprintf(DEBUG_BUFFER, sizeof(DEBUG_BUFFER), "    :    %s", instr.instruction);
        dbg_ui_word_to_hex(current_addr, DEBUG_BUFFER, -1);
        if (nk_select_label(ctx, DEBUG_BUFFER, NK_TEXT_LEFT, false)) {
            /* Label was clicked, do something ... if needed */
        }

        if (pushed) {
            /* If the background color was changed, restore the default one */
            nk_style_pop_color(ctx);
            nk_style_pop_color(ctx);
        }
        nk_layout_row_end(ctx);

        if (instr_bytes == -1) {
            break;
        }

        current_addr += instr_bytes;
    }
}

