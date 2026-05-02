/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * ===========================================================
 *                  SEMIHOST VIEWER
 * ===========================================================
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"
#include "hw/zeal.h"

static uint64_t semihost_counter_total_us(const semihost_t* semihost, const semihost_counter_t* counter)
{
    if (counter->running && semihost->cpu != NULL) {
        uint64_t elapsed = semihost->cpu->cyc - counter->start_cyc;
        return TSTATES_TO_US(elapsed);
    }

    return counter->last_total_us;
}

void ui_panel_semihost(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    (void)panel;
    zeal_t* machine = (zeal_t*)dbg->arg;
    semihost_t* semihost = &machine->semihost;
    struct nk_context* ctx = dctx->ctx;
    const float id_col_width = 24.0f;
    const float row_height = 25.0f;
    const float footer_height = 20.0f;
    char buffer[96];
    struct nk_rect bounds = nk_window_get_content_region(ctx);
    float table_height = bounds.h - footer_height - 4.0f;

    if (table_height < row_height * 2.0f) {
        table_height = row_height * 2.0f;
    }

    nk_layout_row_dynamic(ctx, table_height, 1);
    if (nk_group_begin(ctx, "SemihostCounters", NK_WINDOW_BORDER)) {
        nk_layout_row_template_begin(ctx, row_height);
        nk_layout_row_template_push_static(ctx, id_col_width);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        nk_label(ctx, "#", NK_TEXT_LEFT);
        nk_label(ctx, "us(min, avg, max)", NK_TEXT_RIGHT);
        nk_label(ctx, "ms(min, avg, max)", NK_TEXT_RIGHT);
        nk_label(ctx, "total(us, ms)", NK_TEXT_RIGHT);

        for (uint8_t i = 0; i < SEMIHOST_MAX_COUNTERS; i++) {
            semihost_counter_t* counter = &semihost->counters[i];
            const uint64_t avg_us = counter->sample_count == 0 ? 0 : counter->total_interval_us / counter->sample_count;
            const uint64_t total_us = semihost_counter_total_us(semihost, counter);

            nk_layout_row_template_begin(ctx, row_height);
            nk_layout_row_template_push_static(ctx, id_col_width);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);

            snprintf(buffer, sizeof(buffer), "%u%s%s",
                     i,
                     counter->running ? "*" : "",
                     counter->break_on_update ? "!" : "");
            if (counter->break_on_update) {
                nk_style_push_color(ctx, &ctx->style.selectable.text_normal, nk_rgba(255, 200, 80, 255));
                nk_style_push_color(ctx, &ctx->style.selectable.text_hover, ctx->style.selectable.text_normal);
                dbg_ui_mouse_hover(ctx, MOUSE_POINTER);
                if (nk_select_label(ctx, buffer, NK_TEXT_LEFT, false)) {
                    counter->break_on_update = !counter->break_on_update;
                }
                nk_style_pop_color(ctx);
                nk_style_pop_color(ctx);
            } else {
                if (dbg_ui_clickable_value(ctx, buffer, true)) {
                    counter->break_on_update = !counter->break_on_update;
                }
            }

            snprintf(buffer, sizeof(buffer), "%" PRIu64 ", %" PRIu64 ", %" PRIu64,
                     counter->min_us, avg_us, counter->max_us);
            nk_label(ctx, buffer, NK_TEXT_RIGHT);

            snprintf(buffer, sizeof(buffer), "%" PRIu64 ", %" PRIu64 ", %" PRIu64,
                     counter->min_us / 1000, avg_us / 1000, counter->max_us / 1000);
            nk_label(ctx, buffer, NK_TEXT_RIGHT);

            snprintf(buffer, sizeof(buffer), "%" PRIu64 ", %" PRIu64,
                     total_us, total_us / 1000);
            nk_label(ctx, buffer, NK_TEXT_RIGHT);
        }

        nk_group_end(ctx);
    }

    nk_layout_row_dynamic(ctx, footer_height, 1);
    nk_label(ctx, "* running, ! break on update", NK_TEXT_LEFT);
}
